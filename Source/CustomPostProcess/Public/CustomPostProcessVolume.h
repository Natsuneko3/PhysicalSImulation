// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "CustomPostProcessComponent.h"
#include "RenderAdapter.h"
#include "GameFramework/Volume.h"
#include "TranslucentPP/TranslucentPostprocess.h"
#include "CustomPostProcessVolume.generated.h"

UENUM(BlueprintType)
enum class ECustomPostProcessType : uint8
{
	PostProcessMaterial = 0,
	SNNFilter = 1,
	StylizationLine = 2,
	BilateralBlur = 3,
	Bloom = 4,
	SceneSpaceFluid = 5,
};

USTRUCT(BlueprintType)
struct FCustomPostProcessSet
{
	GENERATED_BODY();

	UPROPERTY(Category = CustomPostProcessVolume,EditAnywhere)
	ECustomPostProcessType Feature;

	UPROPERTY(Category = CustomPostProcessVolume,EditAnywhere,meta=(EditCondition = "Feature==0",EditConditionHides))
	FTranslucentParameterSet PostProcessSetMaterial;
};

UCLASS()
class Utest : public UObject
{
	GENERATED_BODY()
	UPROPERTY(Category =Utest,  EditAnywhere)
	int Test1;
	UPROPERTY(Category =Utest,EditAnywhere)
	int Test2;
public:

};

UCLASS()
class Utest2 : public Utest
{
	GENERATED_BODY()
	UPROPERTY(Category =Utest2,EditAnywhere)
	int Test3;
	UPROPERTY(Category =Utest2,EditAnywhere)
	int Test4;
public:

};

UCLASS(autoexpandcategories=ACustomPostProcessVolume, hidecategories=(Advanced, Collision, Volume, Brush, Attachment))
class CUSTOMPOSTPROCESS_API ACustomPostProcessVolume : public AVolume
{
	GENERATED_BODY()

public:
	// Sets default values for this actor's properties
	ACustomPostProcessVolume();

	UPROPERTY(Category = CustomPostProcessVolume,EditAnywhere,BlueprintReadWrite)
	TArray<TSubclassOf<Utest>> RenderFeatrue;
protected:
	/** The component that defines the transform (location, rotation, scale) of this Actor in the world, all other components must be attached to this one somehow */
	UPROPERTY(Category = Collision, VisibleAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UCustomPostProcessComponent> CPPComponent;

	virtual void BeginPlay() override;

public:
	// Called every frame
	virtual void Tick(float DeltaTime) override;
};
