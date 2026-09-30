#pragma once

#include "CoreMinimal.h"
#include "Sokoban/Data/SokobanTypes.h"

/** 检查关卡初始数据，不修改输入，也不依赖场景、Actor 或编辑器。 */
class SOKOBAN_API FSokobanLevelValidator
{
public:
	/**
	 * 返回发现的问题；空数组表示通过当前的结构校验，不代表谜题一定有解。
	 * 当前所有检查均返回 Error。调用者应按 Severity 判断是否允许开始游戏。
	 * 尺寸或数组长度无效时跳过不安全的地形访问，仍收集可独立判断的问题。
	 */
	static TArray<FSokobanValidationIssue> Validate(const FSokobanLevelDefinition& Level);
};
