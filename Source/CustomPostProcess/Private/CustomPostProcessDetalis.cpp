// Fill out your copyright notice in the Description page of Project Settings.


#include "CustomPostProcessDetalis.h"

#include "ClassViewerFilter.h"
#include "ClassViewerModule.h"
#include "CustomPostProcessVolume.h"
#include "DetailCategoryBuilder.h"
#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "IPropertyUtilities.h"
#include "RenderAdapter.h"
#include "Editor/UnrealEdEngine.h"
#define LOCTEXT_NAMESPACE "CustomPostProcessDetalis"

TSharedRef<IDetailCustomization> FCustomPostProcessDetail::MakeInstance()
{
	return MakeShareable( new FCustomPostProcessDetail );
}

FCustomPostProcessDetail::~FCustomPostProcessDetail()
{
}

void FCustomPostProcessDetail::CustomizeDetails(IDetailLayoutBuilder& InDetailLayout)
{
	IDetailCategoryBuilder& BrushBuilderCategory = InDetailLayout.EditCategory( "CustomPostProcess", FText::GetEmpty() );
	RenderAdapterHandle = InDetailLayout.GetProperty(GET_MEMBER_NAME_CHECKED(ACustomPostProcessVolume, RenderFeatures));
	UObject* BrushBuilderObject = nullptr;
	RenderAdapterHandle->GetValue(BrushBuilderObject);
	if(BrushBuilderObject == nullptr)
	{
		InDetailLayout.HideProperty("RenderFeatures");
	}
	BrushBuilderCategory.AddCustomRow( LOCTEXT("RenderFeatrues", "Render Featrues") )
	.NameContent()
	[
		SNew( STextBlock )
		.Text( LOCTEXT("RenderFeatrue", "Render Featrue"))
		.Font( IDetailLayoutBuilder::GetDetailFont() )
	]
	.ValueContent()
	.MinDesiredWidth(105)
	.MaxDesiredWidth(105)
	[
		SNew(SComboButton)
		.ToolTipText(LOCTEXT("BspModeBuildTooltip", "Rebuild this brush from a parametric builder."))
		.OnGetMenuContent(this, &FCustomPostProcessDetail::GenerateBuildMenuContent)
		.ContentPadding(2)
		.ButtonContent()
		[
			SNew(STextBlock)
			.Text(this, &FCustomPostProcessDetail::GetBuilderText)
			.Font( IDetailLayoutBuilder::GetDetailFont() )
		]
	];

}
FReply FCustomPostProcessDetail::ExecuteExecCommand(FString InCommand)
{
	//GUnrealEd->Exec(GWorld, *InCommand);
	return FReply::Handled();
}

TSharedRef<SWidget> FCustomPostProcessDetail::GenerateBuildMenuContent()
{
	class FBrushFilter : public IClassViewerFilter
	{
	public:
		virtual bool IsClassAllowed(const FClassViewerInitializationOptions& InInitOptions, const UClass* InClass, TSharedRef< class FClassViewerFilterFuncs > InFilterFuncs)
		{
			return !InClass->HasAnyClassFlags(CLASS_Abstract) && InClass->IsChildOf(URenderAdapterBase::StaticClass());
		}

		virtual bool IsUnloadedClassAllowed(const FClassViewerInitializationOptions& InInitOptions, const TSharedRef< const class IUnloadedBlueprintData > InUnloadedClassData, TSharedRef< class FClassViewerFilterFuncs > InFilterFuncs)
		{
			return false;
		}
	};

	FClassViewerInitializationOptions Options;
	Options.ClassFilters.Add(MakeShareable(new FBrushFilter));
	Options.Mode = EClassViewerMode::ClassPicker;
	Options.DisplayMode = EClassViewerDisplayMode::ListView;
	return FModuleManager::LoadModuleChecked<FClassViewerModule>("ClassViewer").CreateClassViewer(Options, FOnClassPicked::CreateSP(this, &FCustomPostProcessDetail::OnClassPicked));
}

void FCustomPostProcessDetail::OnClassPicked(UClass* InChosenClass)
{
	FSlateApplication::Get().DismissAllMenus();

	TArray<UObject*> OuterObjects;
	RenderAdapterHandle->GetOuterObjects(OuterObjects);

	TArray<FString> NewObjectPaths;

	if(RenderAdapterHandle->IsValidHandle() && OuterObjects.Num() > 0)
	{
		const FScopedTransaction Transaction(NSLOCTEXT("UnrealEd", "CustomPostProcessSet", "Custom PostProcess Set"));
		for (UObject* OuterObject : OuterObjects)
		{
			URenderAdapterBase* NewUObject = NewObject<URenderAdapterBase>(OuterObject, InChosenClass, NAME_None, RF_Transactional);
			NewObjectPaths.Add(NewUObject->GetPathName());
		}
		RenderAdapterHandle->SetPerObjectValues(NewObjectPaths);

		GEditor->RebuildAlteredBSP();
		if (PropertyUtils.IsValid())
		{
			PropertyUtils.Pin()->ForceRefresh();
		}
	}
}

FText FCustomPostProcessDetail::GetBuilderText() const
{
	UObject* Object = nullptr;
	RenderAdapterHandle->GetValue(Object);
	if (Object != nullptr)
	{
		URenderAdapterBase* URenderAdapter = CastChecked<URenderAdapterBase>(Object);
		const FText NameText = URenderAdapter->GetClass()->GetDisplayNameText();
		if (!NameText.IsEmpty())
		{
			return NameText;
		}
		else
		{
			return FText::FromString(FName::NameToDisplayString(URenderAdapter->GetClass()->GetName(), false));
		}
	}

	return LOCTEXT("None", "None");
}
