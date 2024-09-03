// Fill out your copyright notice in the Description page of Project Settings.


#include "CustomPostProcessDetalis.h"

#include "DetailCategoryBuilder.h"
#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#define LOCTEXT_NAMESPACE "CustomPostProcessDetalis"

TSharedRef<IDetailCustomization> FCustomPostProcessDetalis::MakeInstance()
{
	return MakeShareable( new FCustomPostProcessDetalis );
}

FCustomPostProcessDetalis::~FCustomPostProcessDetalis()
{
}

void FCustomPostProcessDetalis::CustomizeDetails(IDetailLayoutBuilder& InDetailLayout)
{
	IDetailCategoryBuilder& BrushBuilderCategory = InDetailLayout.EditCategory( "CustomPostProcessSettings", FText::GetEmpty() );

	BrushBuilderCategory.AddProperty( GET_MEMBER_NAME_CHECKED(ABrush, BrushType) );
	BrushBuilderCategory.AddCustomRow( LOCTEXT("RenderFeatrue", "Render Featrue") )
	.NameContent()
	[
		SNew( STextBlock )
		.Text( LOCTEXT("BrushShape", "Brush Shape"))
		.Font( IDetailLayoutBuilder::GetDetailFont() )
	]
	.ValueContent()
	.MinDesiredWidth(105)
	.MaxDesiredWidth(105)
	[
		SNew(SComboButton)
		.ToolTipText(LOCTEXT("BspModeBuildTooltip", "Rebuild this brush from a parametric builder."))
		.OnGetMenuContent(this, &FBrushDetails::GenerateBuildMenuContent)
		.ContentPadding(2)
		.ButtonContent()
		[
			SNew(STextBlock)
			.Text(this, &FBrushDetails::GetBuilderText)
			.Font( IDetailLayoutBuilder::GetDetailFont() )
		]
	];
}
