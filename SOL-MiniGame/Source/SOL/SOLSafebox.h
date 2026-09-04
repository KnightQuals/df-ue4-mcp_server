// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "SOLContainer.h"
#include "SOLSafebox.generated.h"

// 保险箱：高价值小型容器（搜打撤玩法里最肥的点）。
// 与普通容器的差异：
// - 更小的交互半径（要贴得更近才能开，符合"贵重物品藏深处"的直觉）
// - 更厚实的比例
// - 金色材质（M_Gold 由 MCP create_material 创建后经 set_material_on_component
//   挂到实例上，地图保存时随 actor 落盘）
// 物品池走 DataTable 行 Key=SOL_Safebox（宝物向：黄金骷髅/非洲之心/名表/金币）。
UCLASS()
class SOL_API ASOLSafebox : public ASOLContainer
{
	GENERATED_BODY()

public:
	ASOLSafebox();
};
