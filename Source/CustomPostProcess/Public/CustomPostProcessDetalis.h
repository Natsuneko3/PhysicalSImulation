// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "IDetailCustomization.h"
#include "UObject/Object.h"


/**
 * 
 */

class  FCustomPostProcessDetail : public IDetailCustomization
{
public:
	/** Makes a new instance of this detail layout class for a specific detail view requesting it */
	static TSharedRef<IDetailCustomization> MakeInstance();

	~FCustomPostProcessDetail();

	/** IDetailCustomization interface */
	virtual void CustomizeDetails( IDetailLayoutBuilder& InDetailLayout ) override;
private:
	/** Callback for creating a static mesh from valid selected brushes. */

	FReply ExecuteExecCommand(FString InCommand);

	TSharedRef<SWidget> GenerateBuildMenuContent();

	void OnClassPicked(UClass* InChosenClass);

	FText GetBuilderText() const;
private:
	TSharedPtr<IPropertyHandle> RenderAdapterHandle;

	/** Holds a list of BSP brushes or volumes, used for converting to static meshes */
	TArray< TWeakObjectPtr<ABrush> > SelectedBrushes;

	/** Container widget for the geometry mode tools */
	TSharedPtr< SHorizontalBox > GeometryToolsContainer;

	TWeakPtr<IPropertyUtilities> PropertyUtils;
};
