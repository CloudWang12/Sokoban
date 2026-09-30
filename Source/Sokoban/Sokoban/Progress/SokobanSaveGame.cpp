#include "SokobanSaveGame.h"

namespace
{
	bool ValidCounters(const FSokobanMoveCounters& Value)
	{
		return Value.MoveCount >= 0 && Value.PushCount >= 0 && Value.PushCount <= Value.MoveCount;
	}
}

bool USokobanSaveGame::IsValidPayload() const
{
	if (SaveVersion != 1) { return false; }
	for (const auto& Entry : Levels)
	{
		if (Entry.Key.IsNone() || Entry.Value.ContentRevision < 1 || !Entry.Value.bCompleted || !ValidCounters(Entry.Value.Best)) { return false; }
	}
	return true;
}

bool USokobanSaveGame::IsBetter(const FSokobanMoveCounters& Candidate, const FSokobanMoveCounters& Previous)
{
	return Candidate.MoveCount < Previous.MoveCount || (Candidate.MoveCount == Previous.MoveCount && Candidate.PushCount < Previous.PushCount);
}

bool USokobanSaveGame::GetProgress(FName Id, int32 Revision, FSokobanLevelProgress& Out) const
{
	Out = FSokobanLevelProgress();
	const auto* Found = Levels.Find(Id);
	if (!Found || Found->ContentRevision != Revision || !Found->bCompleted) { return false; }
	Out = *Found;
	return true;
}

bool USokobanSaveGame::RecordCompletion(FName Id, int32 Revision, const FSokobanMoveCounters& Counters, bool& bOutNewBest)
{
	bOutNewBest = false;
	if (Id.IsNone() || Revision < 1 || !ValidCounters(Counters)) { return false; }
	FSokobanLevelProgress Existing;
	bOutNewBest = !GetProgress(Id, Revision, Existing) || IsBetter(Counters, Existing.Best);
	if (bOutNewBest)
	{
		FSokobanLevelProgress& Entry = Levels.FindOrAdd(Id);
		Entry.ContentRevision = Revision;
		Entry.bCompleted = true;
		Entry.Best = Counters; // 必须成对保存，不能拼接来自两次解法的最低值。
	}
	LastPlayedLevel = Id;
	return true;
}
