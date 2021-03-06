#pragma once

#include "EngineMinimal.h"
#include "GameFramework/PlayerInput.h"
#include "RequenceStructs.generated.h"

template<typename TEnum>
static FORCEINLINE FString EnumToString(const FString& Name, TEnum Value)
{
	const UEnum* enumPtr = FindObject<UEnum>(ANY_PACKAGE, *Name, true);
	if (!enumPtr)
	{
		return FString("Invalid");
	}
	return enumPtr->GetNameByValue((int64)Value).ToString();
}

template <typename EnumType>
static FORCEINLINE EnumType StringToEnum(const FString& EnumName, const FString& String)
{
	UEnum* Enum = FindObject<UEnum>(ANY_PACKAGE, *EnumName, true);
	if (!Enum)
	{
		return EnumType(0);
	}
	return (EnumType)Enum->GetValueByName(FName(*String));
}

//Device Type
UENUM(BlueprintType)
enum class ERequenceDeviceType : uint8
{
	RDT_Unknown				UMETA(DisplayName = "Unknown"),
	RDT_Keyboard			UMETA(DisplayName = "Keyboard"),
	RDT_Mouse				UMETA(DisplayName = "Mouse"),
	RDT_Gamepad				UMETA(DisplayName = "Gamepad"),
	RDT_MotionController	UMETA(DisplayName = "MotionController"),
	RDT_Unique				UMETA(DisplayName = "Unique Device")
};

//Load errors for save files and 
UENUM(BlueprintType)
enum class ERequenceLoadError : uint8
{
	RLE_Unknown				UMETA(DisplayName = "Unknown"),
	RLE_WrongVersion		UMETA(DisplayName = "Wrong Version"),
	RLE_FileNotFound		UMETA(DisplayName = "File not found")
};

UENUM(BlueprintType)
enum class ERequencePAInputRange : uint8
{
	RPAIR_Default			UMETA(DisplayName = "Default (-1 to 1)"),
	RPAIR_Halved			UMETA(DisplayName = "Halved (0 to 1)"),
	RPAIR_HalvedNegative	UMETA(DisplayName = "Halved (-1 to 0)")
};

USTRUCT(BlueprintType)
struct FRequencePhysicalAxis
{
	GENERATED_USTRUCT_BODY()

public:
	
	//Axis Name
	UPROPERTY(EditAnywhere, BlueprintReadWrite)	FString Axis;

	//Data points of curve editor, scaled -1 to 1, only positive values. No 0 and 1.
	UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FVector2D> DataPoints;

	//Input range setting
	UPROPERTY(EditAnywhere, BlueprintReadWrite) ERequencePAInputRange InputRange;

	//Whether datapoints are precached. DO NOT SAVE IF PRECACHED.
	UPROPERTY() bool bIsPrecached = false;

	FRequencePhysicalAxis() { InputRange = ERequencePAInputRange::RPAIR_Default; }
	FRequencePhysicalAxis(FString _Axis) 
	{
		Axis = _Axis;  
		InputRange = ERequencePAInputRange::RPAIR_Default;
	}

	void PrecacheDatapoints() 
	{
		TArray<FVector2D> Precached;

		//Build new data array, first -1, then flip points and add them as negative. add zero, add points, add 1.
		Precached.Add(FVector2D(-1, -1));
		for (int i = DataPoints.Num()-1; i >= 0; i--) { Precached.Add(FVector2D(DataPoints[i].X * -1, DataPoints[i].Y * -1)); }
		Precached.Add(FVector2D::ZeroVector);
		for (FVector2D vec : DataPoints) { Precached.Add(vec); }
		Precached.Add(FVector2D(1, 1));

		DataPoints = Precached;
		bIsPrecached = true;
	}
};

USTRUCT(BlueprintType)
struct FRequenceInputAction
{
	GENERATED_USTRUCT_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite)	FString ActionName = "";
	UPROPERTY(EditAnywhere, BlueprintReadWrite)	FString KeyString = "";
	UPROPERTY(EditAnywhere, BlueprintReadWrite)	FKey Key;
	UPROPERTY(EditAnywhere, BlueprintReadWrite)	uint32 bShift : 1;
	UPROPERTY(EditAnywhere, BlueprintReadWrite)	uint32 bCtrl : 1;
	UPROPERTY(EditAnywhere, BlueprintReadWrite)	uint32 bAlt : 1;
	UPROPERTY(EditAnywhere, BlueprintReadWrite)	uint32 bCmd : 1;

	FRequenceInputAction() {}
	FRequenceInputAction(const FString InActionName, const FKey InKey, const bool bInShift, const bool bInCtrl, const bool bInAlt, const bool bInCmd)
		: Key(InKey), KeyString(Key.GetDisplayName().ToString()), bShift(bInShift), bCtrl(bInCtrl), bAlt(bInAlt), bCmd(bInCmd)
	{
		ActionName = InActionName;
	}

	FRequenceInputAction(const FInputActionKeyMapping& Action)
		: Key(Action.Key), KeyString(Action.Key.GetDisplayName().ToString()), bShift(Action.bShift), bCtrl(Action.bCtrl), bAlt(Action.bAlt), bCmd(Action.bCmd)
	{
		ActionName = Action.ActionName.ToString();
	}
	FRequenceInputAction(const FString InActionName) : ActionName(InActionName) 
	{
		Key = FKey();
	}
};

USTRUCT(BlueprintType)
struct FRequenceInputAxis
{
	GENERATED_USTRUCT_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite)	FString AxisName = "";
	UPROPERTY(EditAnywhere, BlueprintReadWrite)	FString KeyString = "";
	UPROPERTY(EditAnywhere, BlueprintReadWrite)	FKey Key;
	UPROPERTY(EditAnywhere, BlueprintReadWrite)	float Scale = 1;

	FRequenceInputAxis() {}
	FRequenceInputAxis(const FString InAxisName, FKey InKey, float InScale)
		: AxisName(InAxisName), KeyString(InKey.GetDisplayName().ToString()), Key(InKey), Scale(InScale)
	{ }

	FRequenceInputAxis(const FInputAxisKeyMapping& Axis)
		: Key(Axis.Key), KeyString(Axis.Key.GetDisplayName().ToString()), Scale(Axis.Scale)
	{
		AxisName = Axis.AxisName.ToString();
	}
	FRequenceInputAxis(const FString InAxisName) : AxisName(InAxisName) 
	{
		Key = FKey();
	}
};

/*
*  Danny de Bruijne (2018)
*  RequenceStructs
*
*  Data structs used by Requence
*/
UCLASS()
class URequenceStructs : public UObject
{
	GENERATED_BODY()
public:
	URequenceStructs();

	static float Interpolate(TArray<FVector2D> Points, float val);
};