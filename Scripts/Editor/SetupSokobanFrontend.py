"""生成首次流程演示资产并接入既有 GameMode。已有示例与目录不覆盖。仅编辑器执行。"""
import unreal

TOOLS = unreal.AssetToolsHelpers.get_asset_tools()
LIB = unreal.EditorAssetLibrary
ROOT = '/Game/Sokoban/Levels'

def make_asset(name, cls):
    path = ROOT + '/' + name
    if LIB.does_asset_exist(path):
        asset = LIB.load_asset(path)
        if not isinstance(asset, cls):
            raise RuntimeError('Asset type mismatch: ' + path)
        return asset, False
    factory = unreal.DataAssetFactory()
    factory.set_editor_property('data_asset_class', cls)
    asset = TOOLS.create_asset(name, ROOT, cls, factory)
    if asset is None:
        raise RuntimeError('Create failed: ' + path)
    return asset, True

def point(x, y):
    return unreal.IntPoint(x=x, y=y)

def level(name, identity, title, player, boxes, goals):
    asset, created = make_asset(name, unreal.SokobanLevelData)
    if created:
        definition = unreal.SokobanLevelDefinition()
        definition.set_editor_property('width', 7)
        definition.set_editor_property('height', 5)
        terrain = [unreal.SokobanTerrain.WALL if x in (0, 6) or y in (0, 4) else unreal.SokobanTerrain.FLOOR
                   for y in range(5) for x in range(7)]
        definition.set_editor_property('terrain', terrain)
        definition.set_editor_property('player_spawn', point(*player))
        definition.set_editor_property('goals', [point(*cell) for cell in goals])
        entries = []
        for index, cell in enumerate(boxes):
            entry = unreal.SokobanBoxDefinition()
            entry.set_editor_property('box_id', index)
            entry.set_editor_property('position', point(*cell))
            entries.append(entry)
        definition.set_editor_property('boxes', entries)
        asset.set_editor_property('level_id', identity)
        asset.set_editor_property('display_name', title)
        asset.set_editor_property('definition', definition)
        if not LIB.save_loaded_asset(asset, only_if_is_dirty=False):
            raise RuntimeError('Save failed: ' + name)
    return asset

first = level('DA_Level_001', 'Level_001', '第一关：双箱入位', (2, 3), [(2, 2), (4, 2)], [(2, 1), (4, 1)])
second = level('DA_Level_002', 'Level_002', '第二关：走到箱子后面', (1, 3), [(3, 2)], [(3, 1)])
catalog, created = make_asset('DA_SokobanCatalog', unreal.SokobanLevelCatalog)
if created:
    catalog.set_editor_property('levels', [first, second])
    if not LIB.save_loaded_asset(catalog, only_if_is_dirty=False):
        raise RuntimeError('Catalog save failed')
bp_path = '/Game/Sokoban/Blueprints/BP_SokobanGameMode'
bp = LIB.load_asset(bp_path)
default = unreal.get_default_object(LIB.load_blueprint_class(bp_path))
if default.get_editor_property('level_catalog') is None:
    default.set_editor_property('level_catalog', catalog)
default.set_editor_property('enable_frontend', True)
unreal.BlueprintEditorLibrary.compile_blueprint(bp)
if not LIB.save_loaded_asset(bp, only_if_is_dirty=False):
    raise RuntimeError('GameMode save failed')
unreal.log('SOKOBAN_FRONTEND_SETUP_OK: ' + catalog.get_path_name())
