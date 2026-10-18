# -*- coding: utf-8 -*-
import bpy
import math
import bpy_extras
import gpu
import gpu_extras.batch
import copy
import mathutils
import json

bl_info = {
    "name": "レベルエディタ",
    "author": "Hiroki kato",
    "version": (1, 0),
    "blender": (3, 3, 1),
    "location": "",
    "description": "レベルエディタ",
    "category": "",
    "wiki_url": "",
    "tracker_url": "",
    "category": "Object"
}

#コライダー描画
class DrawCollider:
    # 描画ハンドル
    handle = None

    # 3Dビューに登録する描画関数
    def draw_collider():
        # 頂点データとインデックスデータ
        vertices = {"pos": []}
        indices = []

        offsets = [
            [-0.5, -0.5, -0.5], [+0.5, -0.5, -0.5], [-0.5, +0.5, -0.5], [+0.5, +0.5, -0.5],
            [-0.5, -0.5, +0.5], [+0.5, -0.5, +0.5], [-0.5, +0.5, +0.5], [+0.5, +0.5, +0.5]
        ]

        for object in bpy.context.scene.objects:
            if not "collider" in object:
                continue

            collider_type = object.get("collider_type", object.get("collider", "BOX"))
            center = mathutils.Vector(object.get("collider_center", (0,0,0)))
            start = len(vertices["pos"])

            if collider_type == 'SPHERE':
                radius = object.get("collider_radius", 1.0)
                segments = 16
                for plane in range(3):
                    for i in range(segments):
                        angle = (i / segments) * 2 * math.pi
                        x = math.cos(angle) * radius
                        y = math.sin(angle) * radius
                        pos = copy.copy(center)
                        if plane == 0:
                            pos[0] += x; pos[1] += y
                        elif plane == 1:
                            pos[1] += x; pos[2] += y
                        else:
                            pos[2] += x; pos[0] += y
                        
                        pos = object.matrix_world @ pos
                        vertices["pos"].append(pos)
                        
                        idx = start + plane * segments + i
                        next_idx = start + plane * segments + ((i + 1) % segments)
                        indices.append([idx, next_idx])
            
            else: # AABB, OBB, BOX
                size = mathutils.Vector(object.get("collider_size", (2,2,2)))
                for offset in offsets:
                    pos = copy.copy(center)
                    pos[0] += offset[0] * size[0]
                    pos[1] += offset[1] * size[1]
                    pos[2] += offset[2] * size[2]
                    
                    if collider_type == 'AABB':
                        # AABBは回転させず、スケールと平行移動のみ適用
                        mat_trans = mathutils.Matrix.Translation(object.location)
                        mat_scale = mathutils.Matrix.Scale(object.scale[0], 4, (1,0,0)) @ mathutils.Matrix.Scale(object.scale[1], 4, (0,1,0)) @ mathutils.Matrix.Scale(object.scale[2], 4, (0,0,1))
                        pos = (mat_trans @ mat_scale) @ pos
                    else: # OBB
                        pos = object.matrix_world @ pos
                        
                    vertices["pos"].append(pos)

                indices.extend([
                    [start+0, start+1], [start+2, start+3], [start+0, start+2], [start+1, start+3],
                    [start+4, start+5], [start+6, start+7], [start+4, start+6], [start+5, start+7],
                    [start+0, start+4], [start+1, start+5], [start+2, start+6], [start+3, start+7]
                ])

        if len(vertices["pos"]) == 0:
            return

        shader = gpu.shader.from_builtin('UNIFORM_COLOR')
        batch = gpu_extras.batch.batch_for_shader(shader, 'LINES', vertices, indices=indices)
        color = [0.5, 1.0, 1.0, 1.0]
        shader.bind()
        shader.uniform_float("color", color)
        batch.draw(shader)


# プレイヤー移動限界（ゲーム内設定と完全一致）
PLAYER_LIMIT_X = 35.0      # 左右: ±35.0m (全幅 70m)
PLAYER_LIMIT_Z_MIN = -4.0  # 下限: -4.0m
PLAYER_LIMIT_Z_MAX = 20.0  # 上限: +20.0m (全高 24m)


def get_curve_world_points(curve_obj):
    """CURVEオブジェクトからワールド座標系の頂点列を取得する"""
    try:
        depsgraph = bpy.context.evaluated_depsgraph_get()
        eval_obj = curve_obj.evaluated_get(depsgraph)
        temp_mesh = eval_obj.to_mesh()
        curve_pts = [curve_obj.matrix_world @ v.co for v in temp_mesh.vertices]
        eval_obj.to_mesh_clear()
        return curve_pts
    except Exception:
        return []


def is_rail_player_range_enabled(curve_obj):
    """
    指定されたCURVEオブジェクトでプレイヤー行動範囲の表示が有効化されているかを判定する。
    1. オブジェクトのプロパティまたはカスタムプロパティ 'show_player_range' があればそのブール値
    2. 未設定の場合は、名前が 'rail.01', 'main_rail', 'player' を含むメインレールならTrue、それ以外（敵レール等）はFalse
    """
    if not curve_obj or curve_obj.type != 'CURVE':
        return False
    if hasattr(curve_obj, "show_player_range"):
        return bool(curve_obj.show_player_range)
    if "show_player_range" in curve_obj:
        return bool(curve_obj["show_player_range"])
    name_lower = curve_obj.name.lower()
    if "rail.01" in name_lower or "main" in name_lower or "player" in name_lower:
        return True
    return False


def find_nearest_rail_and_basis(scene, target_pos):
    """
    シーン内のCURVE（レール）オブジェクトから、target_posに最も近いレール上の点、
    および右(best_right)、上(best_up)、前(best_fwd)のローカル基底ベクトルを探索して返す。
    プレイヤー行動範囲が有効なレール（メインレール）を優先して探索する。
    レールが見つからない場合は None を返す。
    """
    all_curves = [o for o in scene.objects if o.type == 'CURVE' and o.visible_get()]
    player_curves = [o for o in all_curves if is_rail_player_range_enabled(o)]
    curves = player_curves if player_curves else all_curves
    if not curves:
        return None

    best_dist = float('inf')
    best_pt = None
    best_right = None
    best_up = None
    best_fwd = None

    for c_obj in curves:
        curve_pts = get_curve_world_points(c_obj)
        if len(curve_pts) < 2:
            continue

        sample_step = max(1, len(curve_pts) // 40)
        sampled_pts = [curve_pts[i] for i in range(0, len(curve_pts), sample_step)]
        if curve_pts and (not sampled_pts or sampled_pts[-1] != curve_pts[-1]):
            sampled_pts.append(curve_pts[-1])

        for idx, p in enumerate(sampled_pts):
            d = (target_pos - p).length_squared
            if d < best_dist:
                best_dist = d
                best_pt = p
                if idx < len(sampled_pts) - 1:
                    fwd = (sampled_pts[idx+1] - p).normalized()
                else:
                    fwd = (p - sampled_pts[idx-1]).normalized()
                up_ref = mathutils.Vector((0, 0, 1))
                if abs(fwd.dot(up_ref)) > 0.95:
                    up_ref = mathutils.Vector((0, 1, 0))
                right = fwd.cross(up_ref).normalized()
                up = right.cross(fwd).normalized()
                best_right = right
                best_up = up
                best_fwd = fwd

    if best_pt is not None:
        return best_pt, best_right, best_up, best_fwd
    return None


# プレイヤー行動範囲リアルタイム描画クラス
class DrawPlayerRange:
    handle = None

    @staticmethod
    def draw_player_range():
        if not bpy.context.scene.get("show_player_move_range", True):
            return

        active_obj = bpy.context.active_object
        if not active_obj:
            return

        shader = gpu.shader.from_builtin('UNIFORM_COLOR')

        # --- 1. レール(CURVE)をクリック（選択）した時：レール沿いの行動範囲トンネルを描画 ---
        # カスタムプロパティ（show_player_range）でチェックされているレールのみ描画
        if active_obj.type == 'CURVE':
            if not is_rail_player_range_enabled(active_obj):
                return
            curve_pts = get_curve_world_points(active_obj)

            if len(curve_pts) >= 2:
                # サンプリング数を適正化（20〜35ステップ）
                sample_step = max(1, len(curve_pts) // 25)
                sampled_pts = [curve_pts[i] for i in range(0, len(curve_pts), sample_step)]
                if curve_pts and (not sampled_pts or sampled_pts[-1] != curve_pts[-1]):
                    sampled_pts.append(curve_pts[-1])

                v_dict = {"pos": []}
                i_list = []

                # トンネル各断面と長手方向フレームの生成
                for idx, p in enumerate(sampled_pts):
                    if idx < len(sampled_pts) - 1:
                        fwd = (sampled_pts[idx+1] - p).normalized()
                    else:
                        fwd = (p - sampled_pts[idx-1]).normalized()

                    up_ref = mathutils.Vector((0, 0, 1))
                    if abs(fwd.dot(up_ref)) > 0.95:
                        up_ref = mathutils.Vector((0, 1, 0))
                    right = fwd.cross(up_ref).normalized()
                    up = right.cross(fwd).normalized()

                    base_v = len(v_dict["pos"])
                    # 4隅 (左下, 右下, 右上, 左上)
                    c0 = p + right * (-PLAYER_LIMIT_X) + up * PLAYER_LIMIT_Z_MIN
                    c1 = p + right * (PLAYER_LIMIT_X)  + up * PLAYER_LIMIT_Z_MIN
                    c2 = p + right * (PLAYER_LIMIT_X)  + up * PLAYER_LIMIT_Z_MAX
                    c3 = p + right * (-PLAYER_LIMIT_X) + up * PLAYER_LIMIT_Z_MAX
                    
                    # 断面中心の十字線
                    m_bot = p + up * PLAYER_LIMIT_Z_MIN
                    m_top = p + up * PLAYER_LIMIT_Z_MAX
                    m_lft = p + right * (-PLAYER_LIMIT_X)
                    m_rgt = p + right * (PLAYER_LIMIT_X)

                    v_dict["pos"].extend([c0, c1, c2, c3, m_bot, m_top, m_lft, m_rgt])

                    # 断面外枠
                    i_list.extend([
                        [base_v+0, base_v+1], [base_v+1, base_v+2],
                        [base_v+2, base_v+3], [base_v+3, base_v+0]
                    ])

                    # 4ステップごとに中心十字線を描画
                    if idx % 4 == 0:
                        i_list.extend([
                            [base_v+4, base_v+5], [base_v+6, base_v+7]
                        ])

                    # 前の断面と繋ぐ4本の長手方向レールライン（トンネルの稜線）
                    if idx > 0:
                        prev_v = base_v - 8
                        i_list.extend([
                            [prev_v+0, base_v+0], [prev_v+1, base_v+1],
                            [prev_v+2, base_v+2], [prev_v+3, base_v+3]
                        ])

                # トンネルを描画（鮮やかなエメラルドグリーン）
                if v_dict["pos"]:
                    batch = gpu_extras.batch.batch_for_shader(shader, 'LINES', v_dict, indices=i_list)
                    shader.bind()
                    shader.uniform_float("color", [0.15, 0.95, 0.65, 0.85])
                    batch.draw(shader)

                # --- レール選択時：シーン内の敵がトンネル内にあるか自動チェック ---
                enemy_objs = [o for o in bpy.context.scene.objects if o.visible_get() and (o.get("is_enemy_flag", False) or "enemy" in o.name.lower())]
                if enemy_objs:
                    warn_v = {"pos": []}
                    warn_i = []
                    ok_v = {"pos": []}
                    ok_i = []

                    for e_obj in enemy_objs:
                        e_pos = e_obj.location
                        # 最寄りのレール点を探索
                        best_dist = float('inf')
                        best_pt = sampled_pts[0]
                        best_right = mathutils.Vector((1, 0, 0))
                        best_up = mathutils.Vector((0, 0, 1))

                        for idx, p in enumerate(sampled_pts):
                            d = (e_pos - p).length_squared
                            if d < best_dist:
                                best_dist = d
                                best_pt = p
                                if idx < len(sampled_pts) - 1:
                                    fwd = (sampled_pts[idx+1] - p).normalized()
                                else:
                                    fwd = (p - sampled_pts[idx-1]).normalized()
                                up_ref = mathutils.Vector((0, 0, 1))
                                if abs(fwd.dot(up_ref)) > 0.95:
                                    up_ref = mathutils.Vector((0, 1, 0))
                                best_right = fwd.cross(up_ref).normalized()
                                best_up = best_right.cross(fwd).normalized()

                        # 敵の相対座標
                        rel = e_pos - best_pt
                        local_x = rel.dot(best_right)
                        local_z = rel.dot(best_up)

                        in_range = (-PLAYER_LIMIT_X <= local_x <= PLAYER_LIMIT_X) and (PLAYER_LIMIT_Z_MIN <= local_z <= PLAYER_LIMIT_Z_MAX)

                        if in_range:
                            # 範囲内マーカー（緑の十字）
                            base = len(ok_v["pos"])
                            sz = 1.8
                            ok_v["pos"].extend([
                                e_pos + best_right * (-sz), e_pos + best_right * sz,
                                e_pos + best_up * (-sz), e_pos + best_up * sz
                            ])
                            ok_i.extend([[base+0, base+1], [base+2, base+3]])
                        else:
                            # 範囲外警告（赤い枠＆最寄りの境界までの接続線）
                            clamped_x = max(-PLAYER_LIMIT_X, min(PLAYER_LIMIT_X, local_x))
                            clamped_z = max(PLAYER_LIMIT_Z_MIN, min(PLAYER_LIMIT_Z_MAX, local_z))
                            border_pt = best_pt + best_right * clamped_x + best_up * clamped_z

                            base = len(warn_v["pos"])
                            sz = 2.2
                            w0 = e_pos + best_right * (-sz) + best_up * (-sz)
                            w1 = e_pos + best_right * (sz)  + best_up * (-sz)
                            w2 = e_pos + best_right * (sz)  + best_up * (sz)
                            w3 = e_pos + best_right * (-sz) + best_up * (sz)
                            warn_v["pos"].extend([w0, w1, w2, w3, e_pos, border_pt])
                            warn_i.extend([
                                [base+0, base+1], [base+1, base+2], [base+2, base+3], [base+3, base+0],
                                [base+4, base+5]
                            ])

                    if ok_v["pos"]:
                        batch = gpu_extras.batch.batch_for_shader(shader, 'LINES', ok_v, indices=ok_i)
                        shader.bind()
                        shader.uniform_float("color", [0.2, 1.0, 0.4, 0.9])
                        batch.draw(shader)

                    if warn_v["pos"]:
                        batch = gpu_extras.batch.batch_for_shader(shader, 'LINES', warn_v, indices=warn_i)
                        shader.bind()
                        shader.uniform_float("color", [1.0, 0.2, 0.2, 1.0])
                        batch.draw(shader)

        # --- 2. 敵オブジェクトが選択されている場合：最寄りレールを基準とした可動枠を表示 ---
        elif active_obj.get("is_enemy_flag", False) or "enemy" in active_obj.name.lower():
            pos = active_obj.location
            info = find_nearest_rail_and_basis(bpy.context.scene, pos)

            if info:
                best_pt, best_right, best_up, best_fwd = info
                rel = pos - best_pt
                fwd_dist = rel.dot(best_fwd)
                slice_center = best_pt + best_fwd * fwd_dist

                local_x = rel.dot(best_right)
                local_z = rel.dot(best_up)
                in_range = (-PLAYER_LIMIT_X <= local_x <= PLAYER_LIMIT_X) and (PLAYER_LIMIT_Z_MIN <= local_z <= PLAYER_LIMIT_Z_MAX)
                box_color = [0.2, 1.0, 0.5, 0.9] if in_range else [1.0, 0.25, 0.2, 1.0]

                v_dict = {"pos": []}
                i_list = []

                # レール上の敵奥行き位置における断面枠
                p0 = slice_center + best_right * (-PLAYER_LIMIT_X) + best_up * PLAYER_LIMIT_Z_MIN
                p1 = slice_center + best_right * (PLAYER_LIMIT_X)  + best_up * PLAYER_LIMIT_Z_MIN
                p2 = slice_center + best_right * (PLAYER_LIMIT_X)  + best_up * PLAYER_LIMIT_Z_MAX
                p3 = slice_center + best_right * (-PLAYER_LIMIT_X) + best_up * PLAYER_LIMIT_Z_MAX

                p_mid_bottom = slice_center + best_up * PLAYER_LIMIT_Z_MIN
                p_mid_top    = slice_center + best_up * PLAYER_LIMIT_Z_MAX
                p_mid_left   = slice_center + best_right * (-PLAYER_LIMIT_X)
                p_mid_right  = slice_center + best_right * (PLAYER_LIMIT_X)

                v_dict["pos"].extend([p0, p1, p2, p3, p_mid_bottom, p_mid_top, p_mid_left, p_mid_right])
                i_list.extend([
                    [0, 1], [1, 2], [2, 3], [3, 0],
                    [4, 5], [6, 7]
                ])

                # レール中心から敵への接続線
                s_idx = len(v_dict["pos"])
                v_dict["pos"].extend([slice_center, pos])
                i_list.append([s_idx, s_idx + 1])

                # 範囲外の場合は最寄り境界点への警告線
                if not in_range:
                    clamped_x = max(-PLAYER_LIMIT_X, min(PLAYER_LIMIT_X, local_x))
                    clamped_z = max(PLAYER_LIMIT_Z_MIN, min(PLAYER_LIMIT_Z_MAX, local_z))
                    border_p = slice_center + best_right * clamped_x + best_up * clamped_z
                    b_idx = len(v_dict["pos"])
                    v_dict["pos"].extend([pos, border_p])
                    i_list.append([b_idx, b_idx + 1])

                batch = gpu_extras.batch.batch_for_shader(shader, 'LINES', v_dict, indices=i_list)
                shader.bind()
                shader.uniform_float("color", box_color)
                batch.draw(shader)
            else:
                x, y, z = pos.x, pos.y, pos.z
                in_range = (-PLAYER_LIMIT_X <= x <= PLAYER_LIMIT_X) and (PLAYER_LIMIT_Z_MIN <= z <= PLAYER_LIMIT_Z_MAX)
                box_color = [0.2, 1.0, 0.5, 0.9] if in_range else [1.0, 0.25, 0.2, 1.0]

                v_dict = {"pos": []}
                i_list = []

                # 敵の奥行き(Y)位置における断面矩形（ワールド原点基準フォールバック）
                p0 = mathutils.Vector((-PLAYER_LIMIT_X, y, PLAYER_LIMIT_Z_MIN))
                p1 = mathutils.Vector((PLAYER_LIMIT_X, y, PLAYER_LIMIT_Z_MIN))
                p2 = mathutils.Vector((PLAYER_LIMIT_X, y, PLAYER_LIMIT_Z_MAX))
                p3 = mathutils.Vector((-PLAYER_LIMIT_X, y, PLAYER_LIMIT_Z_MAX))
                
                p_mid_bottom = mathutils.Vector((0.0, y, PLAYER_LIMIT_Z_MIN))
                p_mid_top = mathutils.Vector((0.0, y, PLAYER_LIMIT_Z_MAX))
                p_mid_left = mathutils.Vector((-PLAYER_LIMIT_X, y, 0.0))
                p_mid_right = mathutils.Vector((PLAYER_LIMIT_X, y, 0.0))

                v_dict["pos"].extend([p0, p1, p2, p3, p_mid_bottom, p_mid_top, p_mid_left, p_mid_right])
                i_list.extend([
                    [0, 1], [1, 2], [2, 3], [3, 0],
                    [4, 5], [6, 7]
                ])

                if not in_range:
                    clamped_x = max(-PLAYER_LIMIT_X, min(PLAYER_LIMIT_X, x))
                    clamped_z = max(PLAYER_LIMIT_Z_MIN, min(PLAYER_LIMIT_Z_MAX, z))
                    nearest_p = mathutils.Vector((clamped_x, y, clamped_z))
                    s_idx = len(v_dict["pos"])
                    v_dict["pos"].extend([pos, nearest_p])
                    i_list.append([s_idx, s_idx + 1])

                batch = gpu_extras.batch.batch_for_shader(shader, 'LINES', v_dict, indices=i_list)
                shader.bind()
                shader.uniform_float("color", box_color)
                batch.draw(shader)


#オペレータ　頂点を伸ばす
class MYADDON_OT_stretch_vertex(bpy.types.Operator):
    bl_idname = "myaddon.stretch_vertex"
    bl_label = "頂点を伸ばす"
    bl_description = "頂点座標を引っ張って伸ばします"
    bl_options = {'REGISTER', 'UNDO'}

    def execute(self, context):
        #ここに頂点を伸ばす処理を書く
        bpy.data.objects["Cube"].data.vertices[0].co.x += 1.0
        print("頂点を伸ばす処理が実行されました。")
        return {'FINISHED'}

#オペレータ　ICO球生成
class MYADDON_OT_create_ico_sphere(bpy.types.Operator):
    bl_idname = "myaddon.create_ico_sphere"
    bl_label = "ICO球生成"
    bl_description = "ICO球を生成します"
    bl_options = {'REGISTER', 'UNDO'}

    def execute(self, context):
        #ここにICO球を生成する処理を書く
        bpy.ops.mesh.primitive_ico_sphere_add()
        print("ICO球が生成されました。")
        return {'FINISHED'}

#オペレータ　Fighter（敵）生成
class MYADDON_OT_create_fighter(bpy.types.Operator):
    bl_idname = "myaddon.create_fighter"
    bl_label = "敵(Fighter)生成"
    bl_description = "敵(Fighter)の配置用ダミーを生成します"
    bl_options = {'REGISTER', 'UNDO'}

    def execute(self, context):
        empty_obj = bpy.data.objects.new("Enemy_Fighter", None)
        empty_obj.empty_display_type = 'CUBE'
        empty_obj.empty_display_size = 2.0
        empty_obj.location = context.scene.cursor.location
        empty_obj["file_name"] = "Fighter"
        empty_obj.is_enemy_flag = True
        empty_obj["is_enemy"] = True
        empty_obj["spawn_progress"] = 0.0
        context.scene.collection.objects.link(empty_obj)
        context.view_layer.objects.active = empty_obj
        empty_obj.select_set(True)
        print("敵(Fighter)ダミーを生成しました。")
        return {'FINISHED'}

#オペレータ　Asteroid（障害物）生成
class MYADDON_OT_create_asteroid(bpy.types.Operator):
    bl_idname = "myaddon.create_asteroid"
    bl_label = "障害物(Asteroid)生成"
    bl_description = "障害物(Asteroid)の配置用ダミーを生成します"
    bl_options = {'REGISTER', 'UNDO'}

    def execute(self, context):
        empty_obj = bpy.data.objects.new("Obstacle_Asteroid", None)
        empty_obj.empty_display_type = 'SPHERE'
        empty_obj.empty_display_size = 3.0
        empty_obj.location = context.scene.cursor.location
        empty_obj["file_name"] = "Asteroid"
        empty_obj["is_obstacle"] = True
        context.scene.collection.objects.link(empty_obj)
        context.view_layer.objects.active = empty_obj
        empty_obj.select_set(True)
        print("障害物(Asteroid)ダミーを生成しました。")
        return {'FINISHED'}

#オペレータ　プレイヤー＆カメラ生成
class MYADDON_OT_create_player_and_camera(bpy.types.Operator):
    bl_idname = "myaddon.create_player_and_camera"
    bl_label = "Player & Camera 配置"
    bl_description = "実際のプレイヤーモデルと、視界（フラスタム）を示すカメラを配置します"
    bl_options = {'REGISTER', 'UNDO'}

    def execute(self, context):
        import os
        import math

        # 1. プレイヤーの読み込み
        player_obj = None
        if "Player" in bpy.data.objects:
            player_obj = bpy.data.objects["Player"]
            player_obj.location = context.scene.cursor.location
        else:
            blend_filepath = bpy.data.filepath
            json_dir = os.path.dirname(blend_filepath) if blend_filepath else ""
            bt_idx = json_dir.find("blender_tools")
            player_path = ""
            if bt_idx != -1:
                player_path = os.path.join(json_dir[:bt_idx], "resources", "3dModels", "player", "player.obj")
            
            if os.path.exists(player_path):
                if hasattr(bpy.ops.wm, "obj_import"):
                    bpy.ops.wm.obj_import(filepath=player_path)
                else:
                    bpy.ops.import_scene.obj(filepath=player_path)
                imported = context.selected_objects
                if imported:
                    player_obj = imported[0]
                    player_obj.name = "Player"
                    player_obj.location = context.scene.cursor.location
                    player_obj.scale = (0.5, 0.5, 0.5) # ゲーム内のスケールに合わせる
                    player_obj["file_name"] = "Player"
            else:
                bpy.ops.mesh.primitive_cube_add(size=2.0)
                player_obj = context.active_object
                player_obj.name = "Player"
                player_obj.scale = (0.5, 0.5, 0.5)
                player_obj["file_name"] = "Player"

        # 2. ゲームカメラの生成
        camera_obj = None
        if "GameCamera" in bpy.data.objects:
            camera_obj = bpy.data.objects["GameCamera"]
        else:
            camera_data = bpy.data.cameras.new(name="GameCameraData")
            camera_data.lens_unit = 'FOV'
            camera_data.sensor_fit = 'VERTICAL' # ゲーム内の垂直45度FOVに合わせる
            camera_data.angle = math.radians(45.0)
            camera_obj = bpy.data.objects.new("GameCamera", camera_data)
            context.scene.collection.objects.link(camera_obj)
        
        # プレイヤーに親子付けしつつ、カメラのワールド座標を指定する
        camera_obj.parent = player_obj
        context.view_layer.update() # 行列更新
        camera_obj.matrix_parent_inverse = player_obj.matrix_world.inverted()
        
        camera_obj.location = (0, -20, 0.2) # ゲームのRailCameraと同じくZ=20の手前(Y=-20)、高さはレール+0.2
        camera_obj.rotation_euler = (math.radians(90), 0, 0) # ワールド空間で+Y方向を向く
        camera_obj["file_name"] = "GameCamera"

        # 3. 視界フラスタム（ワイヤーフレーム四角形）の生成
        frustum_name = "CameraFrustum"
        frustum_obj = None
        if frustum_name in bpy.data.objects:
            frustum_obj = bpy.data.objects[frustum_name]
        else:
            fov_rad = math.radians(45.0) # 垂直FOV
            aspect = 16.0 / 9.0
            distance = 60.0 # 奥のラインまでの距離（大きすぎないように調整）
            half_h = distance * math.tan(fov_rad / 2.0)
            half_w = half_h * aspect
            
            verts = [
                (0, 0, 0),
                (-half_w, -half_h, -distance),
                (half_w, -half_h, -distance),
                (half_w, half_h, -distance),
                (-half_w, half_h, -distance)
            ]
            edges = [
                (0, 1), (0, 2), (0, 3), (0, 4), # カメラからの放射線
                (1, 2), (2, 3), (3, 4), (4, 1)  # 奥の四角形
            ]
            
            mesh = bpy.data.meshes.new(frustum_name)
            mesh.from_pydata(verts, edges, [])
            mesh.update()
            
            frustum_obj = bpy.data.objects.new(frustum_name, mesh)
            context.scene.collection.objects.link(frustum_obj)
        
        frustum_obj.display_type = 'WIRE'
        frustum_obj.parent = camera_obj
        context.view_layer.update()
        frustum_obj.matrix_parent_inverse = mathutils.Matrix.Identity(4) # カメラのローカル空間に完全に一致させる
        frustum_obj.location = (0, 0, 0)
        frustum_obj.rotation_euler = (0, 0, 0)
        frustum_obj["file_name"] = "CameraFrustum"

        # 4. プレイヤー移動範囲枠 (PlayerMoveArea) の生成
        area_name = "PlayerMoveArea"
        area_obj = None
        if area_name in bpy.data.objects:
            area_obj = bpy.data.objects[area_name]
        else:
            # 幅70m (X: -35~+35), 高14m (Z: -4~+10), 奥行き2m
            a_verts = [
                (-PLAYER_LIMIT_X, 0, PLAYER_LIMIT_Z_MIN),
                (PLAYER_LIMIT_X,  0, PLAYER_LIMIT_Z_MIN),
                (PLAYER_LIMIT_X,  0, PLAYER_LIMIT_Z_MAX),
                (-PLAYER_LIMIT_X, 0, PLAYER_LIMIT_Z_MAX),
                (0.0, 0, PLAYER_LIMIT_Z_MIN),
                (0.0, 0, PLAYER_LIMIT_Z_MAX),
                (-PLAYER_LIMIT_X, 0, 0.0),
                (PLAYER_LIMIT_X,  0, 0.0),
            ]
            a_edges = [
                (0, 1), (1, 2), (2, 3), (3, 0),
                (4, 5), (6, 7)
            ]
            a_mesh = bpy.data.meshes.new(area_name)
            a_mesh.from_pydata(a_verts, a_edges, [])
            a_mesh.update()
            area_obj = bpy.data.objects.new(area_name, a_mesh)
            context.scene.collection.objects.link(area_obj)

        area_obj.display_type = 'WIRE'
        area_obj.show_in_front = True
        area_obj.color = (0.2, 1.0, 0.5, 1.0)
        area_obj.parent = player_obj
        context.view_layer.update()
        area_obj.matrix_parent_inverse = player_obj.matrix_world.inverted()
        area_obj.location = (0, 0, 0)
        area_obj.rotation_euler = (0, 0, 0)
        area_obj["file_name"] = "PlayerMoveArea"

        context.view_layer.objects.active = player_obj
        player_obj.select_set(True)

        print("プレイヤー、カメラ、および移動範囲枠(PlayerMoveArea)を生成しました。")
        return {'FINISHED'}

#オペレータ プレイヤー行動範囲ガイド生成 (トンネル/ボックス)
class MYADDON_OT_create_player_range_guide(bpy.types.Operator):
    bl_idname = "myaddon.create_player_range_guide"
    bl_label = "プレイヤー行動範囲ガイド生成"
    bl_description = "プレイヤーの移動可能範囲（幅70m×高14m）を示すガイドトンネルまたはボックスを生成します"
    bl_options = {'REGISTER', 'UNDO'}

    def execute(self, context):
        guide_name = "PlayerRangeGuide"
        if guide_name in bpy.data.objects:
            old_obj = bpy.data.objects[guide_name]
            bpy.data.objects.remove(old_obj, do_unlink=True)

        # レール（CURVE）オブジェクトの検出（プレイヤー行動範囲が有効なレールを優先）
        curve_obj = None
        if context.active_object and context.active_object.type == 'CURVE':
            curve_obj = context.active_object
        else:
            for obj in context.scene.objects:
                if obj.type == 'CURVE' and obj.visible_get() and is_rail_player_range_enabled(obj):
                    curve_obj = obj
                    break
            if not curve_obj:
                for obj in context.scene.objects:
                    if obj.type == 'CURVE' and obj.visible_get():
                        curve_obj = obj
                        break

        verts = []
        edges = []

        if curve_obj:
            depsgraph = context.evaluated_depsgraph_get()
            eval_obj = curve_obj.evaluated_get(depsgraph)
            temp_mesh = eval_obj.to_mesh()
            curve_pts = [curve_obj.matrix_world @ v.co for v in temp_mesh.vertices]
            eval_obj.to_mesh_clear()

            sample_step = max(1, len(curve_pts) // 40)
            sampled_pts = [curve_pts[i] for i in range(0, len(curve_pts), sample_step)]
            if curve_pts and (not sampled_pts or sampled_pts[-1] != curve_pts[-1]):
                sampled_pts.append(curve_pts[-1])

            if len(sampled_pts) >= 2:
                for idx, p in enumerate(sampled_pts):
                    if idx < len(sampled_pts) - 1:
                        fwd = (sampled_pts[idx+1] - p).normalized()
                    else:
                        fwd = (p - sampled_pts[idx-1]).normalized()
                    
                    up_ref = mathutils.Vector((0, 0, 1))
                    if abs(fwd.dot(up_ref)) > 0.95:
                        up_ref = mathutils.Vector((0, 1, 0))
                    right = fwd.cross(up_ref).normalized()
                    up = right.cross(fwd).normalized()

                    base_v = len(verts)
                    c0 = p + right * (-PLAYER_LIMIT_X) + up * PLAYER_LIMIT_Z_MIN
                    c1 = p + right * (PLAYER_LIMIT_X)  + up * PLAYER_LIMIT_Z_MIN
                    c2 = p + right * (PLAYER_LIMIT_X)  + up * PLAYER_LIMIT_Z_MAX
                    c3 = p + right * (-PLAYER_LIMIT_X) + up * PLAYER_LIMIT_Z_MAX
                    verts.extend([c0, c1, c2, c3])

                    edges.extend([
                        (base_v+0, base_v+1), (base_v+1, base_v+2),
                        (base_v+2, base_v+3), (base_v+3, base_v+0)
                    ])

                    if idx > 0:
                        prev_v = base_v - 4
                        edges.extend([
                            (prev_v+0, base_v+0), (prev_v+1, base_v+1),
                            (prev_v+2, base_v+2), (prev_v+3, base_v+3)
                        ])

        # レールがない場合の直方体ガイド
        if not verts:
            center = context.scene.cursor.location
            y_start = center.y - 100.0
            y_steps = [y_start + i * 25.0 for i in range(9)]
            for idx, y in enumerate(y_steps):
                base_v = len(verts)
                c0 = mathutils.Vector((-PLAYER_LIMIT_X, y, PLAYER_LIMIT_Z_MIN))
                c1 = mathutils.Vector((PLAYER_LIMIT_X, y, PLAYER_LIMIT_Z_MIN))
                c2 = mathutils.Vector((PLAYER_LIMIT_X, y, PLAYER_LIMIT_Z_MAX))
                c3 = mathutils.Vector((-PLAYER_LIMIT_X, y, PLAYER_LIMIT_Z_MAX))
                verts.extend([c0, c1, c2, c3])

                edges.extend([
                    (base_v+0, base_v+1), (base_v+1, base_v+2),
                    (base_v+2, base_v+3), (base_v+3, base_v+0)
                ])
                if idx > 0:
                    prev_v = base_v - 4
                    edges.extend([
                        (prev_v+0, base_v+0), (prev_v+1, base_v+1),
                        (prev_v+2, base_v+2), (prev_v+3, base_v+3)
                    ])

        mesh = bpy.data.meshes.new(guide_name)
        mesh.from_pydata(verts, edges, [])
        mesh.update()

        guide_obj = bpy.data.objects.new(guide_name, mesh)
        guide_obj.display_type = 'WIRE'
        guide_obj.show_in_front = True
        guide_obj.color = (0.2, 0.9, 0.5, 1.0)
        guide_obj.hide_render = True
        guide_obj["file_name"] = guide_name
        context.scene.collection.objects.link(guide_obj)

        self.report({'INFO'}, "プレイヤー行動範囲ガイドを生成しました。")
        return {'FINISHED'}

#オペレータ 敵を行動範囲内に収める
class MYADDON_OT_clamp_enemy_to_range(bpy.types.Operator):
    bl_idname = "myaddon.clamp_enemy_to_range"
    bl_label = "敵を行動範囲内に収める"
    bl_description = "選択中の敵オブジェクトの座標を、最寄りの移動レールから見たプレイヤー行動範囲（幅:±35m, 高さ:-4~+10m）内に自動修正します"
    bl_options = {'REGISTER', 'UNDO'}

    def execute(self, context):
        obj = context.active_object
        if not obj:
            return {'CANCELLED'}
        
        loc = obj.location
        info = find_nearest_rail_and_basis(context.scene, loc)
        if info:
            best_pt, best_right, best_up, best_fwd = info
            rel = loc - best_pt
            local_x = rel.dot(best_right)
            local_z = rel.dot(best_up)
            clamped_x = max(-PLAYER_LIMIT_X, min(PLAYER_LIMIT_X, local_x))
            clamped_z = max(PLAYER_LIMIT_Z_MIN, min(PLAYER_LIMIT_Z_MAX, local_z))

            fwd_proj = rel.dot(best_fwd) * best_fwd
            new_loc = best_pt + fwd_proj + best_right * clamped_x + best_up * clamped_z
            obj.location = new_loc
            self.report({'INFO'}, f"移動レール基準で修正しました: 横オフセット={clamped_x:+.1f}m, 縦オフセット={clamped_z:+.1f}m")
        else:
            new_x = max(-PLAYER_LIMIT_X, min(PLAYER_LIMIT_X, loc.x))
            new_z = max(PLAYER_LIMIT_Z_MIN, min(PLAYER_LIMIT_Z_MAX, loc.z))
            obj.location.x = new_x
            obj.location.z = new_z
            self.report({'INFO'}, f"ワールド座標を修正しました: X={new_x:+.1f}, Z={new_z:+.1f}")
        return {'FINISHED'}

#オペレータ　ゲームカメラ生成
class MYADDON_OT_create_game_camera(bpy.types.Operator):
    bl_idname = "myaddon.create_game_camera"
    bl_label = "ゲームカメラ生成"
    bl_description = "ゲーム内の視野角(FOV)を再現したカメラを生成します"
    bl_options = {'REGISTER', 'UNDO'}

    def execute(self, context):
        camera_data = bpy.data.cameras.new(name="GameCameraData")
        camera_data.lens_unit = 'FOV'
        camera_data.angle = math.radians(45.0) # ゲームのFOVに合わせて45度に設定
        camera_obj = bpy.data.objects.new("GameCamera", camera_data)
        camera_obj.location = context.scene.cursor.location
        camera_obj.rotation_euler = (math.radians(90), 0, 0) # 真っ直ぐ前を向くように(Y軸プラス方向)
        context.scene.collection.objects.link(camera_obj)
        context.view_layer.objects.active = camera_obj
        camera_obj.select_set(True)
        print("ゲームカメラを生成しました。")
        return {'FINISHED'}

#オペレータ　カスタムプロパティ['file_name']追加
class MYADDON_OT_add_filename(bpy.types.Operator):
    bl_idname = "myaddon.myaddon_ot_add_filename"
    bl_label = "FileName 追加"
    bl_description = "オブジェクトにカスタムプロパティ['file_name']を追加します"
    bl_options = {'REGISTER', 'UNDO'}

    def execute(self, context):
        #[file_name]プロパティを追加
        context.object["file_name"] = ""

        return {'FINISHED'}

#オペレータ　カスタムプロパティ['collider']追加
class MYADDON_OT_add_collider(bpy.types.Operator):
    bl_idname = "myaddon.myaddon_ot_add_collider"
    bl_label = "コライダー 追加"
    bl_description = "['collider']カスタムプロパティを追加します"
    bl_options = {'REGISTER', 'UNDO'}

    def execute(self, context):
        context.object["collider"] = "SPHERE"
        context.object["collider_type"] = "SPHERE"
        context.object["collider_center"] = mathutils.Vector((0.0, 0.0, 0.0))
        context.object["collider_size"] = mathutils.Vector((2.0, 2.0, 2.0))
        context.object["collider_radius"] = 1.0

        return {'FINISHED'}

#パネル ファイル名
class OBJECT_PT_file_name(bpy.types.Panel):
    """オブジェクトのファイルネームパネル"""
    bl_idname = "OBJECT_PT_file_name"
    bl_label = "ファイル名"
    bl_space_type = 'PROPERTIES'
    bl_region_type = 'WINDOW'
    bl_context = "object"

    def draw(self, context):
        #パネルに項目を追加
        if "file_name" in context.object:
            #既にプロパティがあれば、プロパティを表示
            self.layout.prop(context.object, '["file_name"]', text= self.bl_label)
        else:
            #プロパティがなければ、プロパティ追加のオペレータを表示
            self.layout.operator(MYADDON_OT_add_filename.bl_idname)

#パネル　コライダー
class OBJECT_PT_collider(bpy.types.Panel):
    """オブジェクトのコライダーパネル"""
    bl_idname = "OBJECT_PT_collider"
    bl_label = "コライダー設定"
    bl_space_type = 'PROPERTIES'
    bl_region_type = 'WINDOW'
    bl_context = "object"

    def draw(self, context):
        #パネルに項目を追加
        if "collider" in context.object:
            # カスタムプロパティをUIから直接変更できるようにする
            self.layout.prop(context.object, '["collider_type"]', text="形状 (SPHERE/BOX)")
            self.layout.prop(context.object, '["collider_center"]', text="中心のズレ")
            
            # タイプに応じたプロパティの表示
            c_type = context.object.get("collider_type", "")
            if c_type == 'SPHERE':
                self.layout.prop(context.object, '["collider_radius"]', text="半径")
            else:
                self.layout.prop(context.object, '["collider_size"]', text="サイズ")
        else:
            #プロパティがなければ、プロパティ追加のオペレータを表示
            self.layout.operator(MYADDON_OT_add_collider.bl_idname)

#オペレータ カスタムプロパティ['is_destructible']追加
class MYADDON_OT_add_destructible(bpy.types.Operator):
    bl_idname = "myaddon.myaddon_ot_add_destructible"
    bl_label = "Destructibleフラグ 追加"
    bl_description = "['is_destructible']カスタムプロパティを追加します"
    bl_options = {'REGISTER', 'UNDO'}

    def execute(self, context):
        context.object["is_destructible"] = True
        return {'FINISHED'}

#パネル Destructible
class OBJECT_PT_destructible(bpy.types.Panel):
    """オブジェクトの破壊フラグパネル"""
    bl_idname = "OBJECT_PT_destructible"
    bl_label = "破壊可能フラグ"
    bl_space_type = 'PROPERTIES'
    bl_region_type = 'WINDOW'
    bl_context = "object"

    def draw(self, context):
        if "is_destructible" in context.object:
            self.layout.prop(context.object, '["is_destructible"]', text="破壊可能（チェックで壊れる）")
        else:
            self.layout.operator(MYADDON_OT_add_destructible.bl_idname)

#パネル Enemy Settings
class OBJECT_PT_enemy_settings(bpy.types.Panel):
    """オブジェクトの敵設定パネル"""
    bl_idname = "OBJECT_PT_enemy_settings"
    bl_label = "敵設定 (Enemy Settings)"
    bl_space_type = 'PROPERTIES'
    bl_region_type = 'WINDOW'
    bl_context = "object"

    def draw(self, context):
        obj = context.object
        self.layout.prop(obj, "is_enemy_flag")
        if obj.is_enemy_flag:
            self.layout.prop(obj, "enemy_type", text="敵タイプ")

            # プレイヤー行動範囲の情報とステータス
            box_range = self.layout.box()
            box_range.label(text="【プレイヤー行動可能範囲】", icon='SHADING_BBOX')
            col = box_range.column(align=True)
            col.label(text=f"左右 (X): ±{PLAYER_LIMIT_X:.0f}m (全幅 {PLAYER_LIMIT_X*2:.0f}m)")
            col.label(text=f"上下 (Z): {PLAYER_LIMIT_Z_MIN:.0f}m 〜 +{PLAYER_LIMIT_Z_MAX:.0f}m (全高 {PLAYER_LIMIT_Z_MAX-PLAYER_LIMIT_Z_MIN:.0f}m)")

            # 最寄りレール基準で現在位置をチェック
            info = find_nearest_rail_and_basis(context.scene, obj.location)
            if info:
                best_pt, best_right, best_up, _ = info
                rel = obj.location - best_pt
                local_x = rel.dot(best_right)
                local_z = rel.dot(best_up)
                in_range = (-PLAYER_LIMIT_X <= local_x <= PLAYER_LIMIT_X) and (PLAYER_LIMIT_Z_MIN <= local_z <= PLAYER_LIMIT_Z_MAX)

                col.separator()
                col.label(text=f"最寄りレールからのオフセット:")
                col.label(text=f"  左右: {local_x:+.1f}m (可動域: ±{PLAYER_LIMIT_X:.0f}m)")
                col.label(text=f"  上下: {local_z:+.1f}m (可動域: {PLAYER_LIMIT_Z_MIN:.0f}m〜+{PLAYER_LIMIT_Z_MAX:.0f}m)")
            else:
                x, z = obj.location.x, obj.location.z
                in_range = (-PLAYER_LIMIT_X <= x <= PLAYER_LIMIT_X) and (PLAYER_LIMIT_Z_MIN <= z <= PLAYER_LIMIT_Z_MAX)
                col.separator()
                col.label(text=f"ワールド原点基準 (レール未配置):")
                col.label(text=f"  左右 (X): {x:+.1f}m")
                col.label(text=f"  上下 (Z): {z:+.1f}m")

            if in_range:
                box_range.label(text="✔ プレイヤーの行動範囲内です", icon='CHECKMARK')
            else:
                box_range.label(text="⚠ 範囲外です (倒しにくくなります)", icon='ERROR')
                box_range.operator(MYADDON_OT_clamp_enemy_to_range.bl_idname, text="レール基準の範囲内に収める", icon='SNAP_ON')

            box_range.operator(MYADDON_OT_create_player_range_guide.bl_idname, text="行動範囲ガイド(トンネル)を生成", icon='CURVE_PATH')

            box = self.layout.box()
            box.label(text="【ゲーム側エディタ連携】", icon='INFO')
            box.label(text="モデル・当たり判定・HP・速度・陣形は")
            box.label(text="ゲーム側の「エネミーエディター」で設定されます。")

# オペレータ ボス出現進行度の自動計算
class MYADDON_OT_calc_boss_spawn_progress(bpy.types.Operator):
    bl_idname = "myaddon.calc_boss_spawn_progress"
    bl_label = "本線レールから進行度を自動計算"
    bl_description = "このボスレールの始点に最も近いメインレールの進行度(0.0〜1.0)を自動算出して設定します"
    bl_options = {'REGISTER', 'UNDO'}

    def execute(self, context):
        obj = context.object
        if not obj or obj.type != 'CURVE':
            return {'CANCELLED'}

        pts = get_curve_world_points(obj)
        if not pts:
            self.report({'WARNING'}, "レールの頂点を取得できませんでした")
            return {'CANCELLED'}
        start_pt = pts[0]

        main_curves = [o for o in context.scene.objects if o.type == 'CURVE' and o != obj and is_rail_player_range_enabled(o)]
        if not main_curves:
            main_curves = [o for o in context.scene.objects if o.type == 'CURVE' and o != obj]
        if not main_curves:
            self.report({'WARNING'}, "メインレールが見つかりませんでした")
            return {'CANCELLED'}

        main_obj = main_curves[0]
        main_pts = get_curve_world_points(main_obj)
        if len(main_pts) < 2:
            self.report({'WARNING'}, "メインレールのサンプリングに失敗しました")
            return {'CANCELLED'}

        best_dist = float('inf')
        best_idx = 0
        for i, p in enumerate(main_pts):
            d = (start_pt - p).length_squared
            if d < best_dist:
                best_dist = d
                best_idx = i

        calc_prog = best_idx / float(len(main_pts) - 1)
        obj["spawn_progress"] = round(calc_prog, 3)
        self.report({'INFO'}, f"ボス出現進行度を {obj['spawn_progress']:.3f} ({obj['spawn_progress']*100:.1f}%) に設定しました")
        return {'FINISHED'}

#パネル Rail Settings (レール選択時に行動範囲情報を表示)
class OBJECT_PT_rail_settings(bpy.types.Panel):
    """レールのプレイヤー可動範囲パネル"""
    bl_idname = "OBJECT_PT_rail_settings"
    bl_label = "レール行動範囲設定 (Rail Player Range)"
    bl_space_type = 'PROPERTIES'
    bl_region_type = 'WINDOW'
    bl_context = "object"

    @classmethod
    def poll(cls, context):
        return context.object and context.object.type == 'CURVE'

    def draw(self, context):
        layout = self.layout
        obj = context.object

        box = layout.box()
        box.label(text="【レール種別設定】", icon='CURVE_DATA')
        box.prop(obj, "show_player_range", text="プレイヤー行動範囲を表示する (メインレール)")

        is_enabled = is_rail_player_range_enabled(obj)
        if is_enabled:
            box_range = layout.box()
            box_range.label(text="【レール沿いのプレイヤー行動可能範囲】", icon='SHADING_BBOX')
            col = box_range.column(align=True)
            col.label(text=f"左右 (幅): ±{PLAYER_LIMIT_X:.0f}m (全幅 {PLAYER_LIMIT_X*2:.0f}m)")
            col.label(text=f"上下 (高): {PLAYER_LIMIT_Z_MIN:.0f}m 〜 +{PLAYER_LIMIT_Z_MAX:.0f}m (全高 {PLAYER_LIMIT_Z_MAX-PLAYER_LIMIT_Z_MIN:.0f}m)")
            box_range.label(text="※3Dビュー上でレールに沿った緑のトンネル枠が表示されます", icon='INFO')
            box_range.operator(MYADDON_OT_create_player_range_guide.bl_idname, text="トンネルガイドをメッシュとして生成", icon='CURVE_PATH')
        else:
            box.label(text="※チェックOFF: 行動範囲トンネルは非表示になります (敵レール用)", icon='HIDE_OFF')

        # ボスレール設定セクション
        box_boss = layout.box()
        box_boss.label(text="【ボス戦トリガー設定 (Boss Spawn Settings)】", icon='PIN')
        if "spawn_progress" not in obj:
            obj["spawn_progress"] = 0.60
        box_boss.prop(obj, '["spawn_progress"]', text="出現進行度 (0.0〜1.0)", slider=True)
        box_boss.operator(MYADDON_OT_calc_boss_spawn_progress.bl_idname, text="本線レールから出現位置を自動計算", icon='AUTO')
        prog_val = float(obj.get("spawn_progress", 0.60))
        box_boss.label(text=f"※プレイヤー進行度 {prog_val*100:.1f}% 到達でボス戦が開始します", icon='INFO')

#オペレータ　シーン出力
class MYADDON_OT_export_scene(bpy.types.Operator, bpy_extras.io_utils.ExportHelper):
    bl_idname = "myaddon.myaddon_ot_export_scene"
    bl_label = "シーン出力"
    bl_description = "シーン情報をExportします"
    #出力するファイルの拡張子
    filename_ext = ".json"

    def parse_scene_recursive(self, file, object, level):
        """シーン解析用再帰関数"""

        #オブジェクト名書き込み
        self.write_and_print(file, object.type + " - " + object.name)   
        trans, rot, scale = object.matrix_local.decompose()
        
    def parse_scene_recursive_json(self, data_parent, object, level):
        """シーン解析用再帰関数"""

        # 非表示（目玉アイコンがオフなど）のオブジェクトは出力しない
        if not object.visible_get():
            return

        # ガイド用オブジェクトや視界枠はエクスポートしない
        if object.name.startswith("PlayerRangeGuide") or object.name.startswith("PlayerMoveArea") or object.name.startswith("CameraFrustum"):
            return

        #シーンのオブジェクト1個分のjsonオブジェクト作成
        json_object = dict()
        #オブジェクト種類
        json_object["type"] = object.type
        #オブジェクト名
        json_object["name"] = object.name

        #オブジェクトのローカル座標を分解して取得
        trans, rot, scale = object.matrix_local.decompose()
        #回転をオイラー角に変換して、度数表記に変換
        rot = rot.to_euler()
        rot.x = math.degrees(rot.x)
        rot.y = math.degrees(rot.y)
        rot.z = math.degrees(rot.z)
        #トランスフォーム情報をディクショナリに登録
        transform = dict()
        transform["translation"] = (trans.x, trans.y, trans.z)
        transform["rotation"] = (rot.x, rot.y, rot.z)
        transform["scale"] = (scale.x, scale.y, scale.z)
        #まとめてjsonオブジェクトに登録
        json_object["transform"] = transform

        #カスタムプロパティ'filen_name'
        if "file_name" in object:
            json_object["file_name"] = object["file_name"]

        if "spawn_progress" in object:
            json_object["spawn_progress"] = object["spawn_progress"]

        # 敵フラグと設定（ゲーム側のエネミープリセットマネージャーと連携）
        is_enemy = getattr(object, "is_enemy_flag", False) or object.get("is_enemy", False)
        if is_enemy:
            json_object["is_enemy"] = True
            if hasattr(object, "enemy_type"):
                json_object["enemy_type"] = object.enemy_type
            elif "enemy_type" in object:
                json_object["enemy_type"] = object["enemy_type"]

        # 破壊フラグ
        if "is_destructible" in object:
            json_object["is_destructible"] = bool(object["is_destructible"])
        else:
            json_object["is_destructible"] = True # デフォルトは破壊可能

        #マテリアルの画像テクスチャ名を取得
        if object.type == 'MESH' and len(object.material_slots) > 0:
            mat = object.material_slots[0].material
            if mat and mat.use_nodes:
                for node in mat.node_tree.nodes:
                    if node.type == 'TEX_IMAGE' and node.image:
                        json_object["texture_path"] = node.image.name
                        break

        #カスタムプロパティ'collider'
        if "collider" in object:
            collider = dict()
            c_type = object.get("collider_type", object.get("collider", "BOX"))
            collider["type"] = c_type
            collider["center"] = object["collider_center"].to_list() if "collider_center" in object else [0,0,0]
            if c_type == 'SPHERE':
                collider["radius"] = object.get("collider_radius", 1.0)
            else:
                collider["size"] = object["collider_size"].to_list() if "collider_size" in object else [2,2,2]
            json_object["collider"] = collider

           # カーブ(レール)情報のエクスポート
        if object.type == 'CURVE':
            json_object["curve_points_debug"] = "Script is updated!"
            json_object["show_player_range"] = is_rail_player_range_enabled(object)
            curve_data = object.data
            matrix_world = object.matrix_world
            points_list = []
            for spline in curve_data.splines:
                if spline.type == 'BEZIER':
                    for i, point in enumerate(spline.bezier_points):
                        position = matrix_world @ point.co
                        handle_left = matrix_world @ point.handle_left
                        handle_right = matrix_world @ point.handle_right
                        point_data = {
                            "position": {"x": position.x, "y": position.y, "z": position.z},
                            "handle_left": {"x": handle_left.x, "y": handle_left.y, "z": handle_left.z},
                            "handle_right": {"x": handle_right.x, "y": handle_right.y, "z": handle_right.z},
                            "tilt": point.tilt
                        }
                        # オブジェクト自体に持たせた speed_i, event_i を取得する
                        speed_key = f"speed_{i}"
                        event_key = f"event_{i}"
                        if speed_key in object:
                            point_data["speed"] = object[speed_key]
                        if event_key in object:
                            point_data["event"] = object[event_key]
                        points_list.append(point_data)
                elif spline.type in {'NURBS', 'POLY'}:
                    for point in spline.points:
                        position = matrix_world @ mathutils.Vector((point.co.x, point.co.y, point.co.z))
                        point_data = {
                            "position": {"x": position.x, "y": position.y, "z": position.z},
                            "tilt": point.tilt
                        }
                        points_list.append(point_data)
            
            if points_list:
                json_object["curve_points"] = points_list


        #一個分のjsonオブジェクトを親オブジェクトに登録
        data_parent.append(json_object)

        #子ノードがあれば、子ノード分回す
        if len(object.children) > 0:
            #子ノードリストを作成
            json_object["children"] = list()

            #子ノードへ進む
            for child in object.children:
                self.parse_scene_recursive_json(json_object["children"], child, level + 1)






    def export_json(self):
        """シーン情報をJSON形式で出力"""

        #保存する情報をまとめるdict
        json_object_root = dict()

        #ノード名
        json_object_root["name"] = "Scene"
        #オブジェクトリストを作成
        json_object_root["objects"] = list()

        #シーン内の全オブジェクト走査してバック
        for object in bpy.context.scene.objects:
            #親を持たないオブジェクトはスキップ(代わりに親から呼び出すため)
            if(object.parent):
                continue

            #シーン直下のオブジェクトをルートノード(深さ0)とし、再帰関数で走査
            self.parse_scene_recursive_json(json_object_root["objects"], object, 0)

        #オブジェクトをjson文字列にエンコード
        json_text = json.dumps(json_object_root, ensure_ascii=False, cls=json.JSONEncoder, indent=4)
        
        #コンソールに表示
        print(json_text)

        #ファイルをテキスト形式で書き出し用に開く
        #スコープを抜けると自動で閉じる
        with open(self.filepath, 'wt', encoding='utf-8') as file:

            #ファイルにjson文字列を書き込む
            file.write(json_text)

        # --- OBJファイルの自動エクスポート（無効化） ---
        # 毎回OBJが上書きされるのが不便なため、自動エクスポート機能は停止しました。
        # 必要な時だけ手動でエクスポートしてください。
        '''
        import os
        import mathutils

        # JSONの保存先パスを基準に、resources/3dModels のディレクトリパスを計算
        json_dir = os.path.dirname(self.filepath)
        res_idx = json_dir.find("resources")
        if res_idx != -1:
            models_dir = os.path.join(json_dir[:res_idx], "resources", "3dModels")
        else:
            models_dir = json_dir # 見つからない場合のフォールバック
            
        if not os.path.exists(models_dir):
            os.makedirs(models_dir)

        # 現在の選択状態をクリア (コンテキストエラーを避けるため bpy.ops は使わない)
        for obj in bpy.context.scene.objects:
            obj.select_set(False)

        for object in bpy.context.scene.objects:
            if object.type != 'MESH':
                continue

            # ファイル名の決定
            if "file_name" in object and object["file_name"] != "":
                file_name = object["file_name"]
            
            # カメラやフラスタムは出力しない（PlayerはエクスポートしてC++側で読む）
            if file_name in ["GameCamera", "CameraFrustum"]:
                continue

            # .objを削除してベース名にする
            base_name = file_name.split('.')[0]
            if not file_name.endswith(".obj"):
                file_name += ".obj"

            # モデル用ディレクトリの作成
            obj_dir = os.path.join(models_dir, base_name)
            if not os.path.exists(obj_dir):
                os.makedirs(obj_dir)

            obj_path = os.path.join(obj_dir, file_name)

            # トランスフォームの一時保存
            saved_location = object.location.copy()
            saved_rotation = object.rotation_euler.copy()
            saved_scale = object.scale.copy()
            
            # オブジェクトを選択状態にしてアクティブに
            object.select_set(True)
            bpy.context.view_layer.objects.active = object

            # 原点へ移動、回転とスケールをリセット (純粋なモデルデータのみ出力するため)
            object.location = mathutils.Vector((0.0, 0.0, 0.0))
            object.rotation_euler = mathutils.Euler((0.0, 0.0, 0.0), 'XYZ')
            object.scale = mathutils.Vector((1.0, 1.0, 1.0))
            
            # 内部の更新を強制
            bpy.context.view_layer.update()

            # エクスポート (Blender 4.0以降は wm.obj_export, それ以前は export_scene.obj)
            try:
                if hasattr(bpy.ops.wm, "obj_export"):
                    bpy.ops.wm.obj_export(filepath=obj_path, export_selected_objects=True, export_triangulated_mesh=True, export_normals=True)
                else:
                    bpy.ops.export_scene.obj(filepath=obj_path, use_selection=True, use_triangles=True, use_normals=True)
            except Exception as e:
                print(f"Failed to export {obj_path}: {e}")

            # トランスフォームを復元
            object.location = saved_location
            object.rotation_euler = saved_rotation
            object.scale = saved_scale
            bpy.context.view_layer.update()

            # 選択解除
            object.select_set(False)
        '''






    def export(self):
        """ファイルに出力"""
        print("シーン情報出力開始... %r" % self.filepath)

        with open(self.filepath, 'wt') as file:

            def write_line(text=""):
                print(text)
                file.write(text + "\n")

            #ファイルに文字列を書き込む
            write_line("SCENE")

            def write_object_tree(obj, indent=0):
                ind = "  " * indent
                write_line(f"{ind}{obj.type} - {obj.name}")

                trans, rot, scale = obj.matrix_local.decompose()
                rot = rot.to_euler()
                rot.x = math.degrees(rot.x)
                rot.y = math.degrees(rot.y)
                rot.z = math.degrees(rot.z)

                write_line(f"{ind}  trans({trans.x:.6f}, {trans.y:.6f}, {trans.z:.6f})")
                write_line(f"{ind}  rot({rot.x:.6f}, {rot.y:.6f}, {rot.z:.6f})")
                write_line(f"{ind}  scale({scale.x:.6f}, {scale.y:.6f}, {scale.z:.6f})")
               # ファイル名があれば出力
                if "file_name" in obj:
                    write_line(f'{ind}  file_name("{obj["file_name"]}")')

                # コライダー情報があれば出力
                if "collider" in obj:
                    write_line(f'{ind}  collider("{obj["collider"]}")')
                    c = obj.get("collider_center")
                    s = obj.get("collider_size")
                    if c is not None:
                        write_line(f'{ind}  collider_center({c[0]:.6f}, {c[1]:.6f}, {c[2]:.6f})')
                    if s is not None:
                        write_line(f'{ind}  collider_size({s[0]:.6f}, {s[1]:.6f}, {s[2]:.6f})')

                # 子オブジェクトを再帰的に書き込む
                for child in obj.children:
                    write_object_tree(child, indent + 1)

            # ルートオブジェクト（親を持たないオブジェクト）からツリーを書き込む
            roots = [o for o in bpy.context.scene.objects if o.parent is None]
            for root in roots:
                write_object_tree(root)
                write_line()

    def execute(self, context):
        print("シーン情報をExportします。")

        #ファイルに出力
        self.export_json()

        print("シーン情報をExportしました。")
        self.report({'INFO'}, "シーン情報をExportしました。")

        return {'FINISHED'}


#オペレータ　OBJ出力
class MYADDON_OT_export_objs(bpy.types.Operator):
    bl_idname = "myaddon.myaddon_ot_export_objs"
    bl_label = "OBJ出力"
    bl_description = "メッシュをOBJファイルとして出力します（出力先フォルダや選択対象を指定可能）"
    bl_options = {'REGISTER', 'UNDO'}

    # ディレクトリパス（フォルダ選択）
    directory: bpy.props.StringProperty(
        name="出力先フォルダ",
        description="OBJファイルを出力するディレクトリを選択してください",
        subtype='DIR_PATH'
    )

    # 出力対象
    export_target: bpy.props.EnumProperty(
        name="出力対象",
        description="出力するオブジェクトの範囲を選択してください",
        items=[
            ('SELECTED', "選択中オブジェクトのみ (Selected Only)", "現在3Dビューで選択されているメッシュのみ出力します"),
            ('VISIBLE', "表示中の全オブジェクト (All Visible)", "非表示以外のすべてのメッシュを出力します"),
        ],
        default='SELECTED'
    )

    # モデルごとのサブフォルダ作成
    create_subfolder: bpy.props.BoolProperty(
        name="モデルごとにサブフォルダを作成",
        description="チェックONで [フォルダ名/モデル名.obj] の階層構造で出力します（ゲームエンジンの標準構成）",
        default=True
    )

    # 座標の原点リセット（ローカル原点で出力）
    reset_transform: bpy.props.BoolProperty(
        name="原点(0,0,0)で出力 (Transform Reset)",
        description="配置座標や回転・スケールをリセットし、原点基準のモデルデータとして出力します",
        default=True
    )

    filter_folder: bpy.props.BoolProperty(default=True, options={'HIDDEN'})

    def invoke(self, context, event):
        import os

        # デフォルト出力先フォルダの計算
        if not self.directory:
            blend_filepath = bpy.data.filepath
            if blend_filepath:
                json_dir = os.path.dirname(blend_filepath)
                bt_idx = json_dir.find("blender_tools")
                if bt_idx != -1:
                    self.directory = os.path.join(json_dir[:bt_idx], "resources", "3dModels")
                else:
                    res_idx = json_dir.find("resources")
                    if res_idx != -1:
                        self.directory = os.path.join(json_dir[:res_idx], "resources", "3dModels")
                    else:
                        self.directory = os.path.join(json_dir, "resources", "3dModels")
            else:
                self.directory = "//"

        # 選択中のメッシュオブジェクトがあるかどうかで初期ターゲットを自動判定
        selected_meshes = [obj for obj in context.selected_objects if obj.type == 'MESH']
        if selected_meshes:
            self.export_target = 'SELECTED'
        else:
            self.export_target = 'VISIBLE'

        context.window_manager.fileselect_add(self)
        return {'RUNNING_MODAL'}

    def draw(self, context):
        layout = self.layout
        layout.label(text="【エクスポート設定】", icon='EXPORT')
        box = layout.box()
        box.prop(self, "export_target", expand=True)

        if self.export_target == 'SELECTED':
            sel_count = len([obj for obj in context.selected_objects if obj.type == 'MESH'])
            box.label(text=f"選択中のメッシュ数: {sel_count} 個", icon='RESTRICT_SELECT_OFF')
            if sel_count == 0:
                box.label(text="※メッシュが選択されていません！", icon='ERROR')
        else:
            vis_count = len([obj for obj in context.scene.objects if obj.type == 'MESH' and obj.visible_get()])
            box.label(text=f"出力予定メッシュ数: {vis_count} 個", icon='HIDE_OFF')

        layout.separator()
        layout.prop(self, "create_subfolder")
        layout.prop(self, "reset_transform")

    def execute(self, context):
        print("OBJのExportを開始します。")

        import os
        import mathutils

        models_dir = bpy.path.abspath(self.directory)
        if not os.path.exists(models_dir):
            try:
                os.makedirs(models_dir)
            except Exception as e:
                self.report({'ERROR'}, f"出力先フォルダを作成できませんでした: {e}")
                return {'CANCELLED'}

        # 出力対象オブジェクトの抽出
        if self.export_target == 'SELECTED':
            target_objects = [obj for obj in context.selected_objects if obj.type == 'MESH']
            if not target_objects:
                self.report({'WARNING'}, "出力対象のメッシュが選択されていません。オブジェクトを選択するか、対象を『表示中の全オブジェクト』にしてください。")
                return {'CANCELLED'}
        else:
            target_objects = [obj for obj in context.scene.objects if obj.type == 'MESH' and obj.visible_get()]

        # カメラやフラスタム、行動範囲ガイドなどの補助オブジェクトは除外
        filtered_objects = []
        for obj in target_objects:
            fname = obj.get("file_name", obj.name)
            if fname in ["GameCamera", "CameraFrustum", "PlayerRangeGuide", "PlayerMoveArea"]:
                continue
            if obj.name.startswith("PlayerRangeGuide") or obj.name.startswith("PlayerMoveArea") or obj.name.startswith("CameraFrustum"):
                continue
            filtered_objects.append(obj)

        if not filtered_objects:
            self.report({'WARNING'}, "出力可能なメッシュオブジェクトがありません。")
            return {'CANCELLED'}

        # 元の選択状態・アクティブオブジェクトを退避
        saved_selected = list(context.selected_objects)
        saved_active = context.view_layer.objects.active

        # 現在の選択状態をクリア
        for obj in context.scene.objects:
            obj.select_set(False)

        export_count = 0
        for object in filtered_objects:
            file_name = object.name
            if "file_name" in object and object["file_name"] != "":
                file_name = object["file_name"]

            base_name = file_name.split('.')[0]
            if not file_name.endswith(".obj"):
                file_name += ".obj"

            # サブフォルダを作成するかどうか
            if self.create_subfolder:
                obj_dir = os.path.join(models_dir, base_name)
                if not os.path.exists(obj_dir):
                    os.makedirs(obj_dir)
                obj_path = os.path.join(obj_dir, file_name)
            else:
                obj_path = os.path.join(models_dir, file_name)

            saved_location = object.location.copy()
            saved_rotation = object.rotation_euler.copy()
            saved_scale = object.scale.copy()

            object.select_set(True)
            context.view_layer.objects.active = object

            if self.reset_transform:
                object.location = mathutils.Vector((0.0, 0.0, 0.0))
                object.rotation_euler = mathutils.Euler((0.0, 0.0, 0.0), 'XYZ')
                object.scale = mathutils.Vector((1.0, 1.0, 1.0))
                context.view_layer.update()

            try:
                if hasattr(bpy.ops.wm, "obj_export"):
                    bpy.ops.wm.obj_export(filepath=obj_path, export_selected_objects=True, export_triangulated_mesh=True, export_normals=True, path_mode='COPY')
                else:
                    bpy.ops.export_scene.obj(filepath=obj_path, use_selection=True, use_triangles=True, use_normals=True, path_mode='COPY')
                export_count += 1
            except Exception as e:
                print(f"Failed to export {obj_path}: {e}")

            if self.reset_transform:
                object.location = saved_location
                object.rotation_euler = saved_rotation
                object.scale = saved_scale
                context.view_layer.update()

            object.select_set(False)

        # 選択状態を復元
        for obj in saved_selected:
            if obj.name in context.scene.objects:
                obj.select_set(True)
        if saved_active and saved_active.name in context.scene.objects:
            context.view_layer.objects.active = saved_active

        msg = f"OBJのExportを完了しました。（{export_count}件） -> {models_dir}"
        print(msg)
        self.report({'INFO'}, msg)

        return {'FINISHED'}


#トップバーの拡張メニュー
class TOPBAR_MT_my_menu(bpy.types.Menu):
    bl_idname = "TOPBAR_MT_my_menu"
    bl_label = "MyMenu"
    bl_description = "拡張メニュー by " + bl_info["author"]

    #サブメニューを描画する関数
    def draw(self, context):
        layout = self.layout
        layout.operator("wm.url_open_preset", text="Manual", icon='HELP')
        layout.separator()
        layout.operator(MYADDON_OT_export_scene.bl_idname, text="シーン出力 (template.json)", icon='FILE_TICK')
        layout.separator()
        layout.operator(MYADDON_OT_export_objs.bl_idname, text="OBJ出力 (選択中 / 一括)", icon='EXPORT')


    # サブメニューを追加する関数
    def submenu(self, context):
        #IDを指定でサブメニューを追加
        self.layout.menu(TOPBAR_MT_my_menu.bl_idname)

#登録するクラスリスト
classes  = (
    TOPBAR_MT_my_menu,
    MYADDON_OT_stretch_vertex,
    MYADDON_OT_create_ico_sphere,
    MYADDON_OT_create_fighter,
    MYADDON_OT_create_asteroid,
    MYADDON_OT_create_player_and_camera,
    MYADDON_OT_create_player_range_guide,
    MYADDON_OT_calc_boss_spawn_progress,
    MYADDON_OT_clamp_enemy_to_range,
    MYADDON_OT_create_game_camera,
    MYADDON_OT_export_scene,
    MYADDON_OT_export_objs,
    MYADDON_OT_add_filename,
    OBJECT_PT_file_name,
    MYADDON_OT_add_collider,
    OBJECT_PT_collider,
    MYADDON_OT_add_destructible,
    OBJECT_PT_destructible,
    OBJECT_PT_enemy_settings,
    OBJECT_PT_rail_settings,
)

#登録の関数
def register():

    for cls in classes:
        try:
            bpy.utils.unregister_class(cls)
        except:
            pass

    try:
        bpy.types.TOPBAR_MT_editor_menus.remove(
            TOPBAR_MT_my_menu.submenu
        )
    except:
        pass

    for cls in classes:
        bpy.utils.register_class(cls)
    
    # プロパティの登録（敵設定はゲーム側のEnemyStudioと連携するため、タイプのみ指定）
    bpy.types.Object.is_enemy_flag = bpy.props.BoolProperty(name="敵として配置 (Is Enemy)", default=False)
    bpy.types.Object.enemy_type = bpy.props.EnumProperty(
        items=[
            ('PATROL_H', "左右往復エネミー (PATROL_H)", "左右に行き来して攻撃する敵"),
            ('PATROL_V', "上下往復エネミー (PATROL_V)", "上下に行き来して攻撃する敵"),
            ('RUSHER', "突撃型エネミー (RUSHER)", "突進タイプの小型敵"),
            ('SHOOTER', "射撃型エネミー (SHOOTER)", "弾を発射する敵"),
            ('HOMING', "誘導弾エネミー (HOMING)", "ホーミング弾を撃つ敵"),
            ('TURRET', "地上砲台 (TURRET)", "地上に設置される固定砲台"),
            ('ARMORED_TRAIN_LOCO', "ボス: 装甲列車 (機関車)", "中ボス・装甲列車の先頭車両"),
            ('ARMORED_TRAIN_TURRET', "ボス: 装甲列車 (砲塔車)", "装甲列車の旋回砲塔車"),
            ('ARMORED_TRAIN_MISSILE', "ボス: 装甲列車 (ミサイル車)", "装甲列車のミサイル車"),
        ],
        name="敵タイプ",
        description="ゲーム側エディタで設定した敵プリセットと紐付けられます",
        default='PATROL_H'
    )
    bpy.types.Scene.show_player_move_range = bpy.props.BoolProperty(name="プレイヤー行動範囲を表示", default=True)
    bpy.types.Object.show_player_range = bpy.props.BoolProperty(
        name="プレイヤー行動範囲を表示",
        description="このレール沿いにプレイヤーの行動範囲（トンネル・ガイド）を表示するかどうか。敵レールの場合はオフにしてください",
        default=False
    )

    #メニューに項目を追加
    bpy.types.TOPBAR_MT_editor_menus.append(
        TOPBAR_MT_my_menu.submenu
    )

    #描画関数を3Dビューに追加
    DrawCollider.handle = bpy.types.SpaceView3D.draw_handler_add(DrawCollider.draw_collider, (), 'WINDOW', 'POST_VIEW')
    DrawPlayerRange.handle = bpy.types.SpaceView3D.draw_handler_add(DrawPlayerRange.draw_player_range, (), 'WINDOW', 'POST_VIEW')
    
    print("レベルエディタが有効化されました")


def unregister():

    try:
        bpy.types.TOPBAR_MT_editor_menus.remove(
            TOPBAR_MT_my_menu.submenu
        )
        #描画関数を3Dビューから削除
        if DrawCollider.handle:
            bpy.types.SpaceView3D.draw_handler_remove(DrawCollider.handle, 'WINDOW')
            DrawCollider.handle = None
        if DrawPlayerRange.handle:
            bpy.types.SpaceView3D.draw_handler_remove(DrawPlayerRange.handle, 'WINDOW')
            DrawPlayerRange.handle = None

        del bpy.types.Object.is_enemy_flag
        del bpy.types.Object.enemy_type
        if hasattr(bpy.types.Object, "show_player_range"):
            del bpy.types.Object.show_player_range
        if hasattr(bpy.types.Scene, "show_player_move_range"):
            del bpy.types.Scene.show_player_move_range
    except:
        pass

    for cls in reversed(classes):
        try:
            bpy.utils.unregister_class(cls)
        except:
            pass

    print("レベルエディタが無効化されました")
    
if __name__ == "__main__":
    try:
        unregister()
    except:
        pass

    register()