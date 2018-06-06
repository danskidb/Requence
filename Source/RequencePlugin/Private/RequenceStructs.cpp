// Fill out your copyright notice in the Description page of Project Settings.

#include "RequenceStructs.h"

URequenceStructs::URequenceStructs()
{
}

float URequenceStructs::Interpolate(TArray<FVector2D> Points, float val)
{
	Points.Insert(FVector2D::ZeroVector, 0);
	Points.Add(FVector2D(1, 1));
	//Todo: Add mirror of array for -1.

	for (int i = 1; i < Points.Num(); i++)	//Note, skips first
	{
		if (Points[i].X < val) { continue; }

		float calc = Points[i - 1].Y + (val - Points[i - 1].X) * ((Points[i].Y - Points[i - 1].Y) / (Points[i].X - Points[i - 1].X));
		return calc;
	}

	return val;
}
