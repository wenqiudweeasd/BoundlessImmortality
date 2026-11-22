#!/usr/bin/env node
/*
 * Rewrites Emscripten glue so it passes the syntax check WeChat's servers run on uploaded JavaScript.
 *
 * The check rejects ES2020 syntax (`invalid file: x.js ... Unexpected token .` for `?.`), and the glue from a current
 * Emscripten uses it freely. Each form below is rewritten from the parsed syntax tree, never by text matching, and the
 * result must then parse as ES2017, so anything this script does not know about fails loudly here instead of at upload.
 *
 * Usage: node lower-glue.mjs <file.js>...    (files are rewritten in place; running twice is harmless)
 * The acorn parser comes from the emsdk: set EMSDK, or keep the toolchain in Saved/Codex/UEWasmTools.
 */
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath, pathToFileURL } from 'node:url';

const here = path.dirname(fileURLToPath(import.meta.url));
const emsdk = process.env.EMSDK || path.resolve(here, '../../Saved/Codex/UEWasmTools/emsdk');
const acorn = await import(pathToFileURL(path.join(emsdk, 'upstream/emscripten/node_modules/acorn/dist/acorn.mjs')).href);

function lower(source, fileName) {
  const ast = acorn.parse(source, { ecmaVersion: 'latest', sourceType: 'script' });
  const temps = [];
  const counts = {};
  const bump = kind => { counts[kind] = (counts[kind] || 0) + 1; };
  const fail = (node, what) => {
    const { line, column } = acorn.getLineInfo(source, node.start);
    throw new Error(`${fileName}:${line}:${column}: cannot lower ${what}: ${source.slice(node.start, node.start + 80)}`);
  };
  const newTemp = () => {
    const name = `__lowered${temps.length}`;
    temps.push(name);
    return name;
  };

  function childrenOf(node) {
    const found = [];
    for (const key of Object.keys(node)) {
      const value = node[key];
      if (Array.isArray(value)) {
        for (const item of value) if (item && typeof item.type === 'string') found.push(item);
      } else if (value && typeof value.type === 'string') {
        found.push(value);
      }
    }
    return found.sort((a, b) => a.start - b.start);
  }

  /** Copies the node's source with lowered text substituted for each child. */
  function copy(node) {
    let out = '';
    let position = node.start;
    for (const child of childrenOf(node)) {
      // Shorthand properties list the same range twice.
      if (child.start < position) continue;
      out += source.slice(position, child.start) + emit(child);
      position = child.end;
    }
    return out + source.slice(position, node.end);
  }

  /** An expression that may be evaluated twice without any observable difference. */
  function isRepeatable(node) {
    if (node.type === 'Identifier' || node.type === 'ThisExpression' || node.type === 'Literal') return true;
    if (node.type === 'MemberExpression' && !node.optional) {
      return isRepeatable(node.object) && (!node.computed || isRepeatable(node.property));
    }
    return false;
  }

  function guardObject(node, text, guards) {
    if (isRepeatable(node)) {
      guards.push(text);
      return text;
    }
    const name = newTemp();
    guards.push(`(${name}=${text})`);
    return name;
  }

  function chainLink(node, guards) {
    if (node.type === 'MemberExpression') {
      let object = chainLink(node.object, guards);
      if (node.optional) object = guardObject(node.object, object, guards);
      return object + (node.computed ? `[${emit(node.property)}]` : `.${node.property.name}`);
    }
    if (node.type === 'CallExpression') {
      const callee = chainLink(node.callee, guards);
      if (node.optional) {
        // Evaluating a method's object twice is unsafe, and a temp would lose `this`.
        if (!isRepeatable(node.callee)) fail(node, 'an optional call on a computed callee');
        guards.push(callee);
      }
      return `${callee}(${node.arguments.map(emit).join(',')})`;
    }
    return emit(node);
  }

  function lowerChain(node) {
    bump('optional-chain');
    const guards = [];
    const access = chainLink(node.expression, guards);
    return `(${guards.map(guard => `${guard}==null`).join('||')}?void 0:${access})`;
  }

  function lowerNullish(node) {
    bump('nullish');
    const left = emit(node.left);
    if (isRepeatable(node.left)) return `(${left}!=null?${left}:(${emit(node.right)}))`;
    const name = newTemp();
    return `((${name}=${left})!=null?${name}:(${emit(node.right)}))`;
  }

  function lowerLogicalAssign(node) {
    bump('logical-assign');
    if (!isRepeatable(node.left)) fail(node, 'a logical assignment to a complex target');
    const target = emit(node.left);
    const value = `(${target}=(${emit(node.right)}))`;
    if (node.operator === '??=') return `(${target}!=null?${target}:${value})`;
    return node.operator === '||=' ? `(${target}||${value})` : `(${target}&&${value})`;
  }

  /** Class fields become assignments at the start of the constructor. */
  function lowerClassBody(body, hasSuper) {
    const fields = body.body.filter(member => member.type === 'PropertyDefinition');
    if (!fields.length) return copy(body);
    bump('class-field');
    for (const field of fields) {
      if (field.static || field.computed || field.key.type !== 'Identifier') fail(field, 'a static or computed class field');
    }
    const assignments = fields.map(field => `this.${field.key.name}=${field.value ? emit(field.value) : 'void 0'};`).join('');
    const methods = body.body.filter(member => member.type !== 'PropertyDefinition');
    const constructorMethod = methods.find(member => member.kind === 'constructor');
    const parts = methods.map(member => {
      if (member !== constructorMethod) return emit(member);
      const statements = member.value.body.body;
      if (hasSuper) {
        const first = statements[0];
        const callsSuper = first && first.type === 'ExpressionStatement' && first.expression.type === 'CallExpression' && first.expression.callee.type === 'Super';
        if (!callsSuper) fail(member, 'class fields when super() is not the first statement');
        const bodyStart = member.value.body.start;
        return source.slice(member.start, bodyStart) + '{' + emit(first) + assignments +
          statements.slice(1).map(emit).join('') + '}';
      }
      const bodyStart = member.value.body.start;
      return source.slice(member.start, bodyStart) + '{' + assignments + statements.map(emit).join('') + '}';
    });
    if (!constructorMethod) {
      parts.unshift(hasSuper ? `constructor(...args){super(...args);${assignments}}` : `constructor(){${assignments}}`);
    }
    return `{${parts.join('')}}`;
  }

  function emit(node) {
    switch (node.type) {
      case 'Literal':
        if (node.bigint !== undefined) {
          bump('bigint');
          return `BigInt("${node.bigint}")`;
        }
        break;
      case 'ChainExpression':
        return lowerChain(node);
      case 'LogicalExpression':
        if (node.operator === '??') return lowerNullish(node);
        break;
      case 'AssignmentExpression':
        if (['??=', '||=', '&&='].includes(node.operator)) return lowerLogicalAssign(node);
        break;
      case 'CatchClause':
        if (!node.param) {
          bump('optional-catch');
          return `catch(_unused)${emit(node.body)}`;
        }
        break;
      case 'ClassDeclaration':
      case 'ClassExpression': {
        const id = node.id ? ` ${node.id.name}` : '';
        const parent = node.superClass ? ` extends ${emit(node.superClass)}` : '';
        return `class${id}${parent}${lowerClassBody(node.body, Boolean(node.superClass))}`;
      }
      default:
        break;
    }
    return copy(node);
  }

  const body = emit(ast);
  const declared = temps.length ? `var ${temps.join(',')};` : '';
  const output = declared + body;
  try {
    acorn.parse(output, { ecmaVersion: 2017, sourceType: 'script' });
  } catch (error) {
    throw new Error(`${fileName}: still newer than ES2017 after lowering: ${error.message}`);
  }
  return { output, counts };
}

for (const file of process.argv.slice(2)) {
  const before = fs.readFileSync(file, 'utf8');
  const { output, counts } = lower(before, path.basename(file));
  fs.writeFileSync(file, output);
  console.log(`${path.basename(file)}: ${before.length} -> ${output.length} chars, lowered ${JSON.stringify(counts)}`);
}
