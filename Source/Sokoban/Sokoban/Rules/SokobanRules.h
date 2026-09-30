#pragma once

#include "CoreMinimal.h"
#include "Sokoban/Data/SokobanTypes.h"

/** 纯数据规则：不处理 Enhanced Input、动画、暂停或历史栈，不修改关卡定义。 */
class SOKOBAN_API FSokobanRules
{
public:
	/** 检查一步行动并生成记录，不修改 State。失败时 Record 保持默认值。 */
	static FSokobanMoveResult TryBuildMove(const FSokobanLevelDefinition& Level,const FSokobanBoardState& State,ESokobanDirection Direction);

	/** 重新核对行动合法性、操作前位置及计数；失败返回 false，State 完全不变。 */
	static bool ApplyMove(const FSokobanLevelDefinition& Level,FSokobanBoardState& State,const FSokobanMoveRecord& Record);

	/** 核对操作后状态，并在副本上验证逆操作；失败返回 false，State 完全不变。 */
	static bool RevertMove(const FSokobanLevelDefinition& Level,FSokobanBoardState& State,const FSokobanMoveRecord& Record);

	/** 关卡与当前状态必须合法，且全部箱子位于目标点；非法数据不会被判为通关。 */
	static bool IsSolved(const FSokobanLevelDefinition& Level,const FSokobanBoardState& State);
};
