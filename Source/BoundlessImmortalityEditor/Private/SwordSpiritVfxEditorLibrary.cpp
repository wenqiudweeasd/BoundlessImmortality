#include "SwordSpiritVfxEditorLibrary.h"
#include "InkCultivationEditorLibrary.h"
#include "NiagaraSystem.h"
#include "NiagaraEmitter.h"
#include "NiagaraEmitterHandle.h"
#include "NiagaraScript.h"
#include "NiagaraScriptSource.h"
#include "NiagaraSystemFactoryNew.h"
#include "NiagaraGraph.h"
#include "NiagaraNodeFunctionCall.h"
#include "NiagaraNodeAssignment.h"
#include "NiagaraNodeCustomHlsl.h"
#include "NiagaraNodeOutput.h"
#include "NiagaraSpriteRendererProperties.h"
#include "NiagaraRibbonRendererProperties.h"
#include "EdGraphSchema_Niagara.h"
#include "ViewModels/Stack/NiagaraStackGraphUtilities.h"
#include "ViewModels/Stack/NiagaraParameterHandle.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Engine/StaticMesh.h"
#include "PhysicsEngine/BodySetup.h"
#include "StaticMeshAttributes.h"
#include "MeshDescription.h"
#include "Materials/MaterialInterface.h"
#include "AssetCompilingManager.h"
#include "ShaderCompiler.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Algo/Reverse.h"
#include "UObject/Package.h"
#include "Misc/PackageName.h"

namespace
{
    using FType=FNiagaraTypeDefinition;

    struct FWrite{FType Type;FString Target;FString Code;};

    /**
     * One emitter of the effect spec: a burst (SimpleSpriteBurst, plays once, optionally Delay seconds in) or a rate
     * loop (Fountain, until deactivated, or for Duration seconds). The template's motion modules are disabled; spawn
     * and update then write every attribute with custom HLSL over the parameter map.
     */
    struct FEmitterSpec
    {
        FString Name,Material,Life=TEXT("0.5");
        bool bBurst=true,bRibbon=false,bWorld=false;
        float Count=1.f,Duration=0.f,Delay=0.f;
        ENiagaraSpriteFacingMode Facing=ENiagaraSpriteFacingMode::FaceCamera;
        ENiagaraSpriteAlignment Alignment=ENiagaraSpriteAlignment::Unaligned;
        TArray<FWrite> Spawn,Update;
    };

    /** Warmup pre-simulates looping systems so formations and halos are already there when they appear. */
    struct FSystemSpec{FString Name;TArray<FString> Params;TArray<FEmitterSpec> Emitters;float Warmup=0.f;};

    bool IsVectorParam(const FString& Param){return Param==TEXT("Target")||Param==TEXT("Source");}

    bool ParseType(const FString& Kind,FType& Out)
    {
        if(Kind==TEXT("float"))Out=FType::GetFloatDef();
        else if(Kind==TEXT("int"))Out=FType::GetIntDef();
        else if(Kind==TEXT("vec2"))Out=FType::GetVec2Def();
        else if(Kind==TEXT("vec3"))Out=FType::GetVec3Def();
        else if(Kind==TEXT("vec4"))Out=FType::GetVec4Def();
        else if(Kind==TEXT("color"))Out=FType::GetColorDef();
        else if(Kind==TEXT("pos"))Out=FType::GetPositionDef();
        else return false;
        return true;
    }

    /** Rows are [type, target attribute, HLSL expression]. */
    bool ParseWrites(const FJsonObject& Object,const TCHAR* Field,TArray<FWrite>& Out,FString& Error)
    {
        const TArray<TSharedPtr<FJsonValue>>* Rows=nullptr;
        if(!Object.TryGetArrayField(Field,Rows))return true;
        for(const TSharedPtr<FJsonValue>& Row:*Rows)
        {
            const TArray<TSharedPtr<FJsonValue>>* Cells=nullptr;
            FWrite Write;
            if(!Row->TryGetArray(Cells)||Cells->Num()!=3||!ParseType((*Cells)[0]->AsString(),Write.Type)){Error=FString(Field)+TEXT(" row is not [type, target, code]");return false;}
            Write.Target=(*Cells)[1]->AsString();Write.Code=(*Cells)[2]->AsString();
            Out.Add(MoveTemp(Write));
        }
        return true;
    }

    bool ParseSpec(const FString& Json,TArray<FSystemSpec>& Out,FString& Error)
    {
        TArray<TSharedPtr<FJsonValue>> Systems;
        if(!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json),Systems)){Error=TEXT("spec is not a JSON array");return false;}
        for(const TSharedPtr<FJsonValue>& SystemValue:Systems)
        {
            const TSharedPtr<FJsonObject> System=SystemValue->AsObject();
            const TArray<TSharedPtr<FJsonValue>>* Emitters=nullptr;
            if(!System||!System->TryGetArrayField(TEXT("emitters"),Emitters)){Error=TEXT("system without emitters");return false;}
            FSystemSpec& Spec=Out.AddDefaulted_GetRef();
            Spec.Name=System->GetStringField(TEXT("name"));
            Spec.Warmup=float(System->GetNumberField(TEXT("warmup")));
            System->TryGetStringArrayField(TEXT("params"),Spec.Params);
            for(const TSharedPtr<FJsonValue>& EmitterValue:*Emitters)
            {
                const TSharedPtr<FJsonObject> E=EmitterValue->AsObject();
                if(!E){Error=Spec.Name+TEXT(": emitter is not an object");return false;}
                FEmitterSpec& Emitter=Spec.Emitters.AddDefaulted_GetRef();
                Emitter.Name=E->GetStringField(TEXT("name"));
                Emitter.Material=E->GetStringField(TEXT("mat"));
                Emitter.Life=E->GetStringField(TEXT("life"));
                Emitter.bBurst=E->GetBoolField(TEXT("burst"));
                Emitter.bRibbon=E->GetBoolField(TEXT("ribbon"));
                Emitter.bWorld=E->GetBoolField(TEXT("world"));
                Emitter.Count=float(E->GetNumberField(TEXT("count")));
                Emitter.Duration=float(E->GetNumberField(TEXT("duration")));
                Emitter.Delay=float(E->GetNumberField(TEXT("delay")));
                if(E->GetStringField(TEXT("facing"))==TEXT("custom"))Emitter.Facing=ENiagaraSpriteFacingMode::CustomFacingVector;
                if(E->GetStringField(TEXT("align"))==TEXT("custom"))Emitter.Alignment=ENiagaraSpriteAlignment::CustomAlignment;
                if(!ParseWrites(*E,TEXT("spawn"),Emitter.Spawn,Error)||!ParseWrites(*E,TEXT("update"),Emitter.Update,Error))
                {Error=Spec.Name+TEXT("/")+Emitter.Name+TEXT(": ")+Error;return false;}
            }
        }
        return true;
    }

    struct FGraphEdit
    {
        UNiagaraGraph* Graph=nullptr;
        const UEdGraphSchema_Niagara* Schema=GetDefault<UEdGraphSchema_Niagara>();

        UEdGraphPin* Assign(UNiagaraNodeOutput& Output,const FType& Type,const FString& Target,int32 Index) const
        {
            FNiagaraVariable Variable(Type,*Target);Variable.AllocateData();
            FString Default;Schema->TryGetPinDefaultValueFromNiagaraVariable(Variable,Default);
            UNiagaraNodeAssignment* Node=FNiagaraStackGraphUtilities::AddParameterModuleToStack({Variable},Output,Index,{Default});
            if(!Node)return nullptr;
            const FNiagaraParameterHandle Alias=FNiagaraParameterHandle::CreateAliasedModuleParameterHandle(FNiagaraParameterHandle(*(TEXT("Module.")+Target)),Node);
            return &FNiagaraStackGraphUtilities::GetOrCreateStackFunctionInputOverridePin(*Node,Alias,Type,FGuid(),FGuid());
        }

        void Read(UNiagaraNodeOutput& Output,const FType& Type,const FString& Local,const FString& Source) const
        {
            if(UEdGraphPin* Pin=Assign(Output,Type,Local,0))
            {
                TSet<FNiagaraVariableBase> Known;Known.Add(FNiagaraVariableBase(Type,*Source));
                FNiagaraStackGraphUtilities::SetLinkedParameterValueForFunctionInput(*Pin,FNiagaraVariableBase(Type,*Source),Known);
            }
        }

        bool Expression(UEdGraphPin& Pin,const FType& Type,const FString& Code) const
        {
            UEdGraphPin* Map=nullptr;
            for(UEdGraphPin* Candidate:Pin.GetOwningNode()->Pins)
                if(Candidate->Direction==EGPD_Input&&Schema->PinToTypeDefinition(Candidate).IsSameBaseDefinition(FType::GetParameterMapDef()))Map=Candidate;
            if(!Map||Map->LinkedTo.IsEmpty())return false;
            FGraphNodeCreator<UNiagaraNodeCustomHlsl> Creator(*Graph);
            UNiagaraNodeCustomHlsl* Node=Creator.CreateNode();
            Node->ScriptUsage=ENiagaraScriptUsage::DynamicInput;
            Node->Signature.Inputs={FNiagaraVariable(FType::GetParameterMapDef(),TEXT("Map"))};
            Node->Signature.Outputs={FNiagaraVariable(Type,TEXT("CustomHLSLOutput"))};
            FindFProperty<FStrProperty>(Node->GetClass(),TEXT("CustomHlsl"))->SetPropertyValue_InContainer(Node,Code);
            Creator.Finalize();
            Pin.BreakAllPinLinks();
            UEdGraphPin* In=Node->FindPin(TEXT("Map"),EGPD_Input);
            UEdGraphPin* Out=Node->FindPin(TEXT("CustomHLSLOutput"),EGPD_Output);
            if(!In||!Out)return false;
            In->MakeLinkTo(Map->LinkedTo[0]);Out->MakeLinkTo(&Pin);
            return true;
        }

        bool Write(UNiagaraNodeOutput& Output,const FWrite& Entry) const
        {
            UEdGraphPin* Pin=Assign(Output,Entry.Type,Entry.Target,INDEX_NONE);
            return Pin&&Expression(*Pin,Entry.Type,Entry.Code);
        }

        UNiagaraNodeFunctionCall* Module(const TCHAR* Name) const
        {
            for(UEdGraphNode* Node:Graph->Nodes)if(auto* Call=Cast<UNiagaraNodeFunctionCall>(Node);Call&&Call->GetFunctionName()==Name)return Call;
            return nullptr;
        }

        bool Input(const TCHAR* ModuleName,const TCHAR* InputName,const FType& Type,const FString& Code) const
        {
            UNiagaraNodeFunctionCall* Call=Module(ModuleName);if(!Call)return false;
            const FNiagaraParameterHandle Alias=FNiagaraParameterHandle::CreateAliasedModuleParameterHandle(FNiagaraParameterHandle(*(FString(TEXT("Module."))+InputName)),Call);
            UEdGraphPin& Pin=FNiagaraStackGraphUtilities::GetOrCreateStackFunctionInputOverridePin(*Call,Alias,Type,FGuid(),FGuid());
            return Expression(Pin,Type,Code);
        }

        /** Static switches are pins on the module node; the value is the enum entry whose display name matches. */
        bool Switch(const TCHAR* ModuleName,const TCHAR* InputName,const TCHAR* Display) const
        {
            UNiagaraNodeFunctionCall* Call=Module(ModuleName);
            UEdGraphPin* Pin=Call?Call->FindPin(InputName,EGPD_Input):nullptr;
            const UEnum* Enum=Pin?Cast<UEnum>(Pin->PinType.PinSubCategoryObject.Get()):nullptr;
            if(!Enum)return false;
            for(int32 Index=0;Index<Enum->NumEnums();++Index)
                if(Enum->GetDisplayNameTextByIndex(Index).ToString()==Display){Call->Modify();Pin->DefaultValue=Enum->GetNameStringByIndex(Index);return true;}
            return false;
        }

        /** Plays the emitter state once for Seconds, after which the emitter completes when its particles die. */
        bool Once(float Seconds) const
        {
            const bool bSelf=Switch(TEXT("EmitterState"),TEXT("Life Cycle Mode"),TEXT("Self"));
            const bool bOnce=Switch(TEXT("EmitterState"),TEXT("Loop Behavior"),TEXT("Once"));
            Switch(TEXT("EmitterState"),TEXT("Loop Duration Mode"),TEXT("Fixed"));
            Input(TEXT("EmitterState"),TEXT("Loop Duration"),FType::GetFloatDef(),FString::Printf(TEXT("%.3f"),Seconds));
            return bSelf&&bOnce;
        }

        /**
         * Mirrors User.Color, System.Age, the owner position and every user parameter into Transient.SS* at the top of
         * a stage; custom HLSL may only read map values that an earlier module has linked.
         */
        void ReadParameters(UNiagaraNodeOutput& Output,const FSystemSpec& Spec) const
        {
            Read(Output,FType::GetColorDef(),TEXT("Transient.SSColor"),TEXT("User.Color"));
            Read(Output,FType::GetFloatDef(),TEXT("Transient.SSAge"),TEXT("System.Age"));
            Read(Output,FType::GetPositionDef(),TEXT("Transient.SSOwner"),TEXT("Engine.Owner.Position"));
            for(const FString& Param:Spec.Params)
                Read(Output,IsVectorParam(Param)?FType::GetVec3Def():FType::GetFloatDef(),TEXT("Transient.SS")+Param,TEXT("User.")+Param);
        }
    };

    bool BuildEmitter(UNiagaraSystem& System,const FSystemSpec& Spec,const FEmitterSpec& Emitter,int32 Order,const TMap<FString,UMaterialInterface*>& Materials,TArray<FString>& Log)
    {
        auto Fail=[&](const FString& Why){Log.Add(FString::Printf(TEXT("  %s/%s: %s"),*Spec.Name,*Emitter.Name,*Why));return false;};
        UMaterialInterface* const* Material=Materials.Find(Emitter.Material);
        if(!Material||!*Material)return Fail(TEXT("material ")+Emitter.Material+TEXT(" missing"));
        UNiagaraEmitter* Template=LoadObject<UNiagaraEmitter>(nullptr,Emitter.bBurst
            ?TEXT("/Niagara/DefaultAssets/Templates/Emitters/SimpleSpriteBurst.SimpleSpriteBurst")
            :TEXT("/Niagara/DefaultAssets/Templates/Emitters/Fountain.Fountain"));
        if(!Template)return Fail(TEXT("template missing"));
        const FNiagaraEmitterHandle Handle=System.AddEmitterHandle(*Template,FName(*Emitter.Name),Template->GetExposedVersion().VersionGuid);
        const FVersionedNiagaraEmitter Instance=Handle.GetInstance();
        FVersionedNiagaraEmitterData* Data=Instance.GetEmitterData();
        if(!Data)return Fail(TEXT("no emitter data"));
        Data->bLocalSpace=!Emitter.bWorld;
        Data->SimTarget=ENiagaraSimTarget::CPUSim;
        // Custom-HLSL reads through the map are invisible to the attribute trimmer: keep what spawn stores for update.
        for(const FWrite& Entry:Emitter.Spawn)if(Entry.Target.StartsWith(TEXT("Particles.")))Data->AttributesToPreserve.AddUnique(Entry.Target);
        auto Renderers=Data->GetRenderers();
        if(Emitter.bRibbon)
        {
            for(UNiagaraRendererProperties* Renderer:Renderers)Instance.Emitter->RemoveRenderer(Renderer,Instance.Version);
            UNiagaraRibbonRendererProperties* Ribbon=NewObject<UNiagaraRibbonRendererProperties>(Instance.Emitter,NAME_None,RF_Transactional);
            Ribbon->Material=*Material;Ribbon->SortOrderHint=Order;
            Instance.Emitter->AddRenderer(Ribbon,Instance.Version);
        }
        else for(UNiagaraRendererProperties* Renderer:Renderers)if(auto* Sprite=Cast<UNiagaraSpriteRendererProperties>(Renderer))
        {
            Sprite->Material=*Material;Sprite->FacingMode=Emitter.Facing;Sprite->Alignment=Emitter.Alignment;Sprite->SortOrderHint=Order;
        }

        auto* Source=Cast<UNiagaraScriptSource>(Data->UpdateScriptProps.Script->GetLatestSource());
        FGraphEdit Edit;Edit.Graph=Source?Source->NodeGraph:nullptr;
        if(!Edit.Graph)return Fail(TEXT("no graph"));
        for(UEdGraphNode* Node:Edit.Graph->Nodes)if(auto* Call=Cast<UNiagaraNodeFunctionCall>(Node))
        {
            const FString Function=Call->GetFunctionName();
            if(Function==TEXT("SolveForcesAndVelocity")||Function==TEXT("GravityForce")||Function==TEXT("Drag")||Function==TEXT("ScaleColor")||Function==TEXT("AddVelocity")||Function==TEXT("ShapeLocation"))
                FNiagaraStackGraphUtilities::SetModuleIsEnabled(*Call,false);
        }

        const FType Float=FType::GetFloatDef();
        if(Emitter.bBurst)
        {
            // One short loop: the burst fires Delay seconds in and the emitter completes when its particles die.
            if(!Edit.Once(FMath::Max(.1f,Emitter.Delay+.05f)))Fail(TEXT("warning: emitter state switches not found, burst may loop"));
            if(!Edit.Input(TEXT("SpawnBurst_Instantaneous"),TEXT("Spawn Count"),FType::GetIntDef(),FString::FromInt(FMath::RoundToInt(Emitter.Count))))return Fail(TEXT("spawn count"));
            if(Emitter.Delay>0.f&&!Edit.Input(TEXT("SpawnBurst_Instantaneous"),TEXT("Spawn Time"),Float,FString::Printf(TEXT("%.3f"),Emitter.Delay)))Fail(TEXT("warning: burst delay ignored"));
        }
        else
        {
            if(!Edit.Input(TEXT("SpawnRate"),TEXT("SpawnRate"),Float,FString::Printf(TEXT("%.3f"),Emitter.Count)))return Fail(TEXT("spawn rate"));
            if(Emitter.Duration>0.f&&!Edit.Once(Emitter.Duration))Fail(TEXT("warning: emitter state switches not found, loop keeps running"));
        }

        UNiagaraNodeOutput* Spawn=Edit.Graph->FindEquivalentOutputNode(ENiagaraScriptUsage::ParticleSpawnScript);
        UNiagaraNodeOutput* Update=Edit.Graph->FindEquivalentOutputNode(ENiagaraScriptUsage::ParticleUpdateScript);
        if(!Spawn||!Update)return Fail(TEXT("missing particle stages"));
        Edit.ReadParameters(*Spawn,Spec);
        if(!Edit.Write(*Spawn,{Float,TEXT("Particles.Lifetime"),Emitter.Life}))return Fail(TEXT("lifetime"));
        for(const FWrite& Entry:Emitter.Spawn)if(!Edit.Write(*Spawn,Entry))return Fail(TEXT("spawn write ")+Entry.Target);
        Edit.ReadParameters(*Update,Spec);
        for(const FWrite& Entry:Emitter.Update)if(!Edit.Write(*Update,Entry))return Fail(TEXT("write ")+Entry.Target);
        Edit.Graph->NotifyGraphChanged();
        return true;
    }

    void CollectErrors(const UNiagaraScript* Script,const FString& Tag,TArray<FString>& Log)
    {
        if(!Script)return;
        const FNiagaraVMExecutableData& Exec=Script->GetVMExecutableData();
        if(!Exec.ErrorMsg.IsEmpty())Log.Add(FString::Printf(TEXT("  %s: %s"),*Tag,*Exec.ErrorMsg.Left(900)));
        for(const FNiagaraCompileEvent& Event:Exec.LastCompileEvents)
            if(Event.Severity==FNiagaraCompileEventSeverity::Error)Log.Add(FString::Printf(TEXT("  %s: %s"),*Tag,*Event.Message.Left(700)));
    }

    float DefaultFor(const FString& Param)
    {
        if(Param==TEXT("Size"))return 120.f;
        if(Param==TEXT("Width"))return 12.f;
        if(Param==TEXT("Length"))return 200.f;
        if(Param==TEXT("Life"))return .45f;
        return 100.f;
    }

    void BuildSystem(const FString& Folder,const FSystemSpec& Spec,const TMap<FString,UMaterialInterface*>& Materials,TArray<FString>& Log)
    {
        UPackage* Package=CreatePackage(*(Folder/Spec.Name));
        if(FindObject<UObject>(Package,*Spec.Name)){Log.Add(Spec.Name+TEXT(": already exists; delete it before rebuilding"));return;}
        UNiagaraSystem* System=NewObject<UNiagaraSystem>(Package,*Spec.Name,RF_Public|RF_Standalone|RF_Transactional);
        UNiagaraSystemFactoryNew::InitializeSystem(System,true);
        auto& Store=System->GetExposedParameters();
        Store.SetParameterValue(FLinearColor(1.f,1.f,1.f,1.f),FNiagaraVariable(FType::GetColorDef(),TEXT("User.Color")),true);
        for(const FString& Param:Spec.Params)
        {
            if(IsVectorParam(Param))Store.SetParameterValue(FVector3f::ZeroVector,FNiagaraVariable(FType::GetVec3Def(),*(TEXT("User.")+Param)),true);
            else Store.SetParameterValue(DefaultFor(Param),FNiagaraVariable(FType::GetFloatDef(),*(TEXT("User.")+Param)),true);
        }
        TArray<FString> Details;bool bBuilt=true;
        for(int32 Index=0;Index<Spec.Emitters.Num();++Index)bBuilt&=BuildEmitter(*System,Spec,Spec.Emitters[Index],Index,Materials,Details);
        const bool bCompiled=bBuilt&&UInkCultivationEditorLibrary::CompileSystem(System);
        CollectErrors(System->GetSystemSpawnScript(),Spec.Name+TEXT(" system spawn"),Details);
        CollectErrors(System->GetSystemUpdateScript(),Spec.Name+TEXT(" system update"),Details);
        for(const FNiagaraEmitterHandle& Handle:System->GetEmitterHandles())
        {
            const FVersionedNiagaraEmitterData* Data=Handle.GetInstance().GetEmitterData();if(!Data)continue;
            CollectErrors(Data->SpawnScriptProps.Script,Spec.Name+TEXT("/")+Handle.GetName().ToString()+TEXT(" spawn"),Details);
            CollectErrors(Data->UpdateScriptProps.Script,Spec.Name+TEXT("/")+Handle.GetName().ToString()+TEXT(" update"),Details);
        }
        System->SetWarmupTickDelta(1.f/30.f);
        System->SetWarmupTime(Spec.Warmup);
        FAssetRegistryModule::AssetCreated(System);
        System->MarkPackageDirty();
        const bool bClean=!Details.ContainsByPredicate([](const FString& Line){return !Line.Contains(TEXT("warning:"));});
        Log.Add(FString::Printf(TEXT("%s: %s, %d emitters"),*Spec.Name,bCompiled&&bClean?TEXT("ok"):TEXT("FAILED"),System->GetNumEmitters()));
        Log.Append(Details);
    }
}

void USwordSpiritVfxEditorLibrary::FinishPendingCompilation()
{
    FAssetCompilingManager::Get().FinishAllCompilation();
    if(GShaderCompilingManager)GShaderCompilingManager->FinishAllCompilation();
}

TArray<FString> USwordSpiritVfxEditorLibrary::BuildSwordSpiritEffects(const FString& Folder,const TMap<FString,UMaterialInterface*>& Materials,const FString& Spec)
{
    TArray<FString> Log;
    if(!Folder.StartsWith(TEXT("/Game/"))){Log.Add(TEXT("Effects must be built under /Game"));return Log;}
    TArray<FSystemSpec> Systems;FString Error;
    if(!ParseSpec(Spec,Systems,Error)){Log.Add(TEXT("Bad effect spec: ")+Error);return Log;}
    for(const FSystemSpec& System:Systems)BuildSystem(Folder,System,Materials,Log);
    return Log;
}

FString USwordSpiritVfxEditorLibrary::NormalizeSwordMesh(UStaticMesh* Mesh,float Length,int32 MaxTriangles)
{
    if(!Mesh||Mesh->GetNumSourceModels()<1||Length<=0.f)return TEXT("no mesh");
    FMeshDescription* Description=Mesh->GetMeshDescription(0);
    if(!Description||Description->Vertices().Num()==0)return TEXT("no mesh description");
    FStaticMeshAttributes Attributes(*Description);
    TVertexAttributesRef<FVector3f> Positions=Attributes.GetVertexPositions();
    FBox3f Box(ForceInit);
    for(const FVertexID Vertex:Description->Vertices().GetElementIDs())Box+=Positions[Vertex];
    const FVector3f Extent=Box.GetSize();
    int32 Axis=2;if(Extent.X>Extent[Axis])Axis=0;if(Extent.Y>Extent[Axis])Axis=1;
    if(Extent[Axis]<=UE_KINDA_SMALL_NUMBER)return TEXT("degenerate mesh");
    const int32 U=(Axis+1)%3,V=(Axis+2)%3;

    // Cross-section extents in slices along the long axis: the widest slice is the guard, and the blade runs
    // from it toward the far end. A guard near the middle is ambiguous; then the narrower end is the tip.
    constexpr int32 Slices=48;
    float MinU[Slices],MaxU[Slices],MinV[Slices],MaxV[Slices];
    for(int32 I=0;I<Slices;++I){MinU[I]=MinV[I]=TNumericLimits<float>::Max();MaxU[I]=MaxV[I]=-TNumericLimits<float>::Max();}
    for(const FVertexID Vertex:Description->Vertices().GetElementIDs())
    {
        const FVector3f P=Positions[Vertex];
        const int32 S=FMath::Clamp(int32((P[Axis]-Box.Min[Axis])/Extent[Axis]*Slices),0,Slices-1);
        MinU[S]=FMath::Min(MinU[S],P[U]);MaxU[S]=FMath::Max(MaxU[S],P[U]);
        MinV[S]=FMath::Min(MinV[S],P[V]);MaxV[S]=FMath::Max(MaxV[S],P[V]);
    }
    auto Width=[&](int32 I){return MaxU[I]>=MinU[I]?FMath::Max(MaxU[I]-MinU[I],MaxV[I]-MinV[I]):-1.f;};
    int32 Guard=Slices/2;float Widest=-1.f;
    for(int32 I=0;I<Slices;++I)if(Width(I)>Widest){Widest=Width(I);Guard=I;}
    auto EndWidth=[&](int32 From,int32 Step){for(int32 I=From;I>=0&&I<Slices;I+=Step)if(Width(I)>=0.f)return Width(I);return 0.f;};
    const float GuardT=(Guard+.5f)/Slices;
    const bool bByGuard=FMath::Abs(GuardT-.5f)>.08f;
    const float Sign=bByGuard?(GuardT<.5f?1.f:-1.f):(EndWidth(Slices-1,-1)<=EndWidth(0,1)?1.f:-1.f);
    FVector3f Origin;
    Origin[Axis]=Box.Min[Axis]+GuardT*Extent[Axis];
    Origin[U]=(MinU[Guard]+MaxU[Guard])*.5f;
    Origin[V]=(MinV[Guard]+MaxV[Guard])*.5f;

    // Proper rotations only (det +1), so tangent frames and winding survive.
    auto Rotate=[Axis,Sign](const FVector3f& P)->FVector3f
    {
        switch(Axis)
        {
        case 0:return FVector3f(-Sign*P.Z,P.Y,Sign*P.X);
        case 1:return FVector3f(P.X,-Sign*P.Z,Sign*P.Y);
        default:return FVector3f(P.X,Sign*P.Y,Sign*P.Z);
        }
    };
    const float Scale=Length/Extent[Axis];
    Mesh->Modify();
    for(const FVertexID Vertex:Description->Vertices().GetElementIDs())Positions[Vertex]=Rotate(Positions[Vertex]-Origin)*Scale;
    TVertexInstanceAttributesRef<FVector3f> Normals=Attributes.GetVertexInstanceNormals();
    TVertexInstanceAttributesRef<FVector3f> Tangents=Attributes.GetVertexInstanceTangents();
    for(const FVertexInstanceID Instance:Description->VertexInstances().GetElementIDs())
    {
        Normals[Instance]=Rotate(Normals[Instance]);
        Tangents[Instance]=Rotate(Tangents[Instance]);
    }
    const int32 Triangles=Description->Triangles().Num();
    Mesh->CommitMeshDescription(0);
    FStaticMeshSourceModel& Model=Mesh->GetSourceModel(0);
    Model.ReductionSettings.PercentTriangles=MaxTriangles>0&&Triangles>MaxTriangles?float(MaxTriangles)/float(Triangles):1.f;
    // Import-time collision no longer matches the moved vertices; the blades never collide anyway.
    if(UBodySetup* Body=Mesh->GetBodySetup()){Body->Modify();Body->RemoveSimpleCollision();}
    Mesh->Build(false);
    Mesh->PostEditChange();
    Mesh->MarkPackageDirty();
    static const TCHAR* AxisNames[]={TEXT("X"),TEXT("Y"),TEXT("Z")};
    return FString::Printf(TEXT("%s: long axis %s, tip %s by %s (guard slice %d/%d), %.1f -> %.0f cm, triangles %d (keep %.0f%%)"),
        *Mesh->GetName(),AxisNames[Axis],Sign>0?TEXT("+"):TEXT("-"),bByGuard?TEXT("guard"):TEXT("narrow end"),Guard,Slices,Extent[Axis],Length,Triangles,Model.ReductionSettings.PercentTriangles*100.f);
}

UStaticMesh* USwordSpiritVfxEditorLibrary::CreateGiantAxeMesh(const FString& PackagePath,UMaterialInterface* Material)
{
    if(!PackagePath.StartsWith(TEXT("/Game/")))return nullptr;
    UPackage* Package=CreatePackage(*PackagePath);
    const FString Name=FPackageName::GetLongPackageAssetName(PackagePath);
    if(FindObject<UObject>(Package,*Name))return nullptr;

    FMeshDescription Description;
    FStaticMeshAttributes Attributes(Description);
    Attributes.Register();
    TVertexAttributesRef<FVector3f> Positions=Attributes.GetVertexPositions();
    TVertexInstanceAttributesRef<FVector2f> UVs=Attributes.GetVertexInstanceUVs();
    UVs.SetNumChannels(1);
    const FPolygonGroupID Group=Description.CreatePolygonGroup();
    Attributes.GetPolygonGroupMaterialSlotNames()[Group]=TEXT("Axe");

    // Flat-shaded faces with their own vertices; Outward orients the winding like the engine's triangle normal.
    auto Face=[&](TArray<FVector3f> Points,const FVector3f& Outward)
    {
        if((((Points[2]-Points[0])^(Points[1]-Points[0]))|Outward)<0.f)Algo::Reverse(Points);
        TArray<FVertexInstanceID> Corners;
        for(const FVector3f& P:Points)
        {
            const FVertexID Vertex=Description.CreateVertex();Positions[Vertex]=P;
            const FVertexInstanceID Instance=Description.CreateVertexInstance(Vertex);
            UVs.Set(Instance,0,FVector2f((P.X+P.Y)/320.f,P.Z/320.f));
            Corners.Add(Instance);
        }
        Description.CreatePolygon(Group,Corners);
    };
    // Convex prism around Z.
    auto Prism=[&](int32 Sides,float Radius,float Z0,float Z1)
    {
        TArray<FVector3f> Bottom,Top;
        for(int32 I=0;I<Sides;++I)
        {
            const float Angle=PI/Sides+2.f*PI*I/Sides;
            const FVector3f Ring(FMath::Cos(Angle)*Radius,FMath::Sin(Angle)*Radius,0.f);
            Bottom.Add(Ring+FVector3f(0.f,0.f,Z0));Top.Add(Ring+FVector3f(0.f,0.f,Z1));
        }
        for(int32 I=0;I<Sides;++I)
        {
            const int32 J=(I+1)%Sides;const FVector3f Mid=(Bottom[I]+Bottom[J])*.5f;
            Face({Bottom[I],Bottom[J],Top[J],Top[I]},FVector3f(Mid.X,Mid.Y,0.f));
        }
        Face(Top,FVector3f(0.f,0.f,1.f));Face(Bottom,FVector3f(0.f,0.f,-1.f));
    };
    // Convex X/Z outline extruded along Y; thickness tapers from the shaft side toward the cutting edge.
    auto Plate=[&](const TArray<FVector2f>& Outline,float HalfInner,float HalfEdge,float EdgeFrom,float EdgeTo)
    {
        auto Half=[&](float X){return FMath::Lerp(HalfInner,HalfEdge,FMath::Clamp((FMath::Abs(X)-EdgeFrom)/(EdgeTo-EdgeFrom),0.f,1.f));};
        TArray<FVector3f> Front,Back;
        FVector2f Center(0.f,0.f);
        for(const FVector2f& P:Outline){Front.Add(FVector3f(P.X,Half(P.X),P.Y));Back.Add(FVector3f(P.X,-Half(P.X),P.Y));Center+=P;}
        Center/=float(Outline.Num());
        Face(Front,FVector3f(0.f,1.f,0.f));Face(Back,FVector3f(0.f,-1.f,0.f));
        for(int32 I=0;I<Outline.Num();++I)
        {
            const int32 J=(I+1)%Outline.Num();
            const FVector2f Mid=(Outline[I]+Outline[J])*.5f-Center;
            Face({Front[I],Front[J],Back[J],Back[I]},FVector3f(Mid.X,0.f,Mid.Y));
        }
    };

    Prism(8,6.5f,0.f,300.f);                                       // haft
    Prism(8,9.5f,-6.f,10.f);                                       // pommel
    Prism(8,8.5f,200.f,212.f);Prism(8,8.5f,272.f,284.f);           // collars around the head
    {
        // Crown spike.
        const FVector3f Apex(0.f,0.f,345.f);
        const FVector3f Base[4]={FVector3f(7.f,7.f,300.f),FVector3f(-7.f,7.f,300.f),FVector3f(-7.f,-7.f,300.f),FVector3f(7.f,-7.f,300.f)};
        for(int32 I=0;I<4;++I){const FVector3f Mid=(Base[I]+Base[(I+1)%4])*.5f;Face({Base[I],Base[(I+1)%4],Apex},FVector3f(Mid.X,Mid.Y,2.f));}
        Face({Base[0],Base[1],Base[2],Base[3]},FVector3f(0.f,0.f,-1.f));
    }
    {
        // Crescent blade: straight inner side on the haft, an arc edge with a long beard below.
        TArray<FVector2f> Blade;
        Blade.Add(FVector2f(6.f,215.f));
        for(int32 I=0;I<=12;++I){const float Angle=FMath::DegreesToRadians(-52.f+104.f*I/12.f);Blade.Add(FVector2f(60.f+80.f*FMath::Cos(Angle),245.f+80.f*FMath::Sin(Angle)));}
        Blade.Add(FVector2f(6.f,275.f));
        Plate(Blade,5.f,.8f,20.f,110.f);
        // Back spike.
        Plate({FVector2f(-6.f,228.f),FVector2f(-52.f,245.f),FVector2f(-6.f,262.f)},4.5f,.8f,10.f,50.f);
    }

    UStaticMesh* Mesh=NewObject<UStaticMesh>(Package,*Name,RF_Public|RF_Standalone|RF_Transactional);
    Mesh->GetStaticMaterials().Add(FStaticMaterial(Material,TEXT("Axe"),TEXT("Axe")));
    FStaticMeshSourceModel& Model=Mesh->AddSourceModel();
    Model.BuildSettings.bRecomputeNormals=true;
    Model.BuildSettings.bRecomputeTangents=true;
    Model.BuildSettings.bGenerateLightmapUVs=false;
    FMeshDescription* Target=Mesh->CreateMeshDescription(0);
    *Target=MoveTemp(Description);
    Mesh->CommitMeshDescription(0);
    Mesh->Build(false);
    Mesh->PostEditChange();
    FAssetRegistryModule::AssetCreated(Mesh);
    Mesh->MarkPackageDirty();
    return Mesh;
}
