"""在 UE 编辑器 Python 环境中创建灰盒默认材质；已有资产不覆盖。

生成的 .uasset 需要随 Content 提交。游戏运行及其他人编译项目不依赖本脚本。
"""

import unreal


def create_default_material():
    asset_path = "/Game/Sokoban/Materials/M_SokobanDefault"
    if unreal.EditorAssetLibrary.does_asset_exist(asset_path):
        unreal.log("Sokoban default material already exists; leaving it unchanged.")
        return

    material = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        "M_SokobanDefault", "/Game/Sokoban/Materials", unreal.Material, unreal.MaterialFactoryNew()
    )
    if material is None:
        raise RuntimeError("Unable to create Sokoban default material")

    # 一个基础材质供六种棋盘元素共享，通过运行时材质实例分别设置颜色。
    material.set_editor_property("used_with_instanced_static_meshes", True)
    library = unreal.MaterialEditingLibrary
    color = library.create_material_expression(material, unreal.MaterialExpressionVectorParameter, -500, -120)
    color.set_editor_property("parameter_name", "BaseColor")
    color.set_editor_property("default_value", unreal.LinearColor(0.18, 0.21, 0.27, 1.0))
    roughness = library.create_material_expression(material, unreal.MaterialExpressionConstant, -250, 140)
    roughness.set_editor_property("r", 0.85)
    glow = library.create_material_expression(material, unreal.MaterialExpressionScalarParameter, -500, 280)
    glow.set_editor_property("parameter_name", "EmissiveStrength")
    glow.set_editor_property("default_value", 0.0)
    multiply = library.create_material_expression(material, unreal.MaterialExpressionMultiply, -200, 300)

    connections = [
        library.connect_material_property(color, "", unreal.MaterialProperty.MP_BASE_COLOR),
        library.connect_material_property(roughness, "", unreal.MaterialProperty.MP_ROUGHNESS),
        library.connect_material_expressions(color, "", multiply, "A"),
        library.connect_material_expressions(glow, "", multiply, "B"),
        library.connect_material_property(multiply, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR),
    ]
    if not all(connections):
        raise RuntimeError("Unable to connect Sokoban material expressions")
    library.recompile_material(material)
    if not unreal.EditorAssetLibrary.save_loaded_asset(material):
        raise RuntimeError("Unable to save Sokoban default material")
    unreal.log("Created Sokoban default material: " + asset_path)


create_default_material()
