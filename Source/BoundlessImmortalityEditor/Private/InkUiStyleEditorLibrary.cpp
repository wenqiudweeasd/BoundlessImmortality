#include "InkUiStyleEditorLibrary.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "ObjectTools.h"
#include "AssetToolsModule.h"
#include "UObject/ObjectRedirector.h"
#include "UObject/SavePackage.h"
#include "Misc/PackageName.h"

bool UInkUiStyleEditorLibrary::RelocateUiAsset(UObject* Asset,const FString& DestinationPackage)
{
    if(!Asset||!Asset->GetOutermost()->GetName().StartsWith(TEXT("/Game/UI/"))||!DestinationPackage.StartsWith(TEXT("/Game/UI/Ink/"))||!FPackageName::IsValidLongPackageName(DestinationPackage))return false;
    UPackage* Old=Asset->GetOutermost();Old->FullyLoad();
    ObjectTools::FPackageGroupName Target;Target.PackageName=DestinationPackage;Target.ObjectName=Asset->GetName();
    TSet<UPackage*> Refused;FText Error;
    if(!ObjectTools::RenameSingleObject(Asset,Target,Refused,Error,nullptr,true))
    {UE_LOG(LogTemp,Error,TEXT("UI relocation failed: %s"),*Error.ToString());return false;}
    FSavePackageArgs Args;Args.TopLevelFlags=RF_Public|RF_Standalone;Args.SaveFlags=SAVE_NoError;
    const FString OldFile=FPackageName::LongPackageNameToFilename(Old->GetName(),FPackageName::GetAssetPackageExtension());
    const FString NewFile=FPackageName::LongPackageNameToFilename(DestinationPackage,FPackageName::GetAssetPackageExtension());
    return UPackage::SavePackage(Asset->GetOutermost(),Asset,*NewFile,Args)&&UPackage::SavePackage(Old,nullptr,*OldFile,Args);
}

void UInkUiStyleEditorLibrary::FixUiRedirectors()
{
    auto& Registry=FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
    TArray<FAssetData> Assets;Registry.GetAssetsByPath(FName(TEXT("/Game/UI")),Assets,true);
    TArray<UObjectRedirector*> Redirectors;
    for(const auto& Data:Assets)if(Data.AssetClassPath==UObjectRedirector::StaticClass()->GetClassPathName())
        if(auto* Redirector=Cast<UObjectRedirector>(Data.GetAsset()))Redirectors.Add(Redirector);
    FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools")).Get().FixupReferencers(Redirectors,false);
}
