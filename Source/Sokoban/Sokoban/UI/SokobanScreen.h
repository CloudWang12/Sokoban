#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/Button.h"
#include "SokobanUIData.h"
#include "SokobanScreen.generated.h"

class USokobanFlowSubsystem;

UENUM(BlueprintType)
enum class ESokobanUIAction : uint8 { Select, MainMenu, StartLevel, Replay, Next, Undo, Redo, Restart, Save, Repair, Quit };

/** 仅保留旧蓝图 Execute.Source 引脚的类型兼容；新界面使用普通 UMG Button。 */
UCLASS(NotBlueprintable, meta=(DisplayName="Sokoban Action Button (Legacy)"))
class SOKOBAN_API USokobanActionButton : public UButton
{
	GENERATED_BODY()
};

/** 数据与命令桥接层。禁止在此创建控件、设置样式或依赖蓝图控件名称。 */
UCLASS(Abstract, Blueprintable)
class SOKOBAN_API USokobanScreen : public UUserWidget
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintPure, Category="Sokoban|UI") 
	FSokobanUIViewData GetViewData() const;
	
	UFUNCTION(BlueprintPure, Category="Sokoban|UI") 
	TArray<FSokobanLevelListEntry> GetLevelEntries() const;
	
	/** 数据变更时才发通知；Force 用于蓝图重新绑定后的主动同步。 */
	UFUNCTION(BlueprintCallable, Category="Sokoban|UI") 
	void RefreshViewData(bool bForce = false);
	
	/** 返回后端是否接受请求；需要确认的请求返回 false，并发出确认事件。 */
	UFUNCTION(BlueprintCallable, Category="Sokoban|UI")
	bool RequestAction(ESokobanUIAction Action, int32 Index = -1);
	
	/** 兼容旧蓝图调用。Source 不再参与显示或确认逻辑，新绑定推荐 RequestAction。 */
	UFUNCTION(BlueprintCallable, Category="Sokoban|UI", meta=(AdvancedDisplay="Source"))
	void Execute(ESokobanUIAction Action, int32 Index = -1, USokobanActionButton* Source = nullptr);
	
	UFUNCTION(BlueprintCallable, Category="Sokoban|UI") 
	bool ResolveConfirmation(bool bConfirmed);
	
	UFUNCTION(BlueprintPure, Category="Sokoban|UI") 
	bool HasPendingConfirmation() const { return bConfirmationPending; }
	
	UFUNCTION(BlueprintPure, Category="Sokoban|UI")
	ESokobanUIAction GetPendingAction() const { return PendingAction; }
	
	/** 蓝图实现：根据快照更新自行设计的界面，不应在该事件中发起游戏命令。 */
	UFUNCTION(BlueprintImplementableEvent, Category="Sokoban|UI")
	void OnViewDataChanged(const FSokobanUIViewData& Data);
	
	/** 蓝图实现：显示自定义确认框；确认／取消分别调用 ResolveConfirmation(true/false)。 */
	UFUNCTION(BlueprintImplementableEvent, Category="Sokoban|UI") 
	void OnConfirmationRequested(ESokobanUIAction Action, const FText& Reason);
	
	
	DECLARE_EVENT_OneParam(USokobanScreen, FViewDataChanged, const FSokobanUIViewData&);
	FViewDataChanged& OnViewDataChangedNative() { return ViewDataChanged; }
	
protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& Geometry, float DeltaSeconds) override;
private:
	USokobanFlowSubsystem* Flow() const;
	void RequestConfirmation(ESokobanUIAction Action, const FText& Reason);
	bool CanIssueAction() const;
	UPROPERTY(Transient) FSokobanUIViewData LastData;
	FViewDataChanged ViewDataChanged;
	ESokobanUIAction PendingAction = ESokobanUIAction::Select;
	bool bConfirmationPending = false;
	bool bHasData = false;
	bool bActive = false;
	bool bHandlingAction = false;
	bool bNotifying = false;
};

// 保留四个独立父类便于资产识别；它们均不含任何默认布局。
UCLASS(Abstract, Blueprintable)
class SOKOBAN_API USokobanMainMenuWidget : public USokobanScreen { GENERATED_BODY() };
UCLASS(Abstract, Blueprintable)
class SOKOBAN_API USokobanLevelSelectWidget : public USokobanScreen { GENERATED_BODY() };
UCLASS(Abstract, Blueprintable)
class SOKOBAN_API USokobanPlayWidget : public USokobanScreen { GENERATED_BODY() };
UCLASS(Abstract, Blueprintable)
class SOKOBAN_API USokobanResultsWidget : public USokobanScreen { GENERATED_BODY() };
