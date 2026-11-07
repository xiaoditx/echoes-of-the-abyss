# 数据配表 Schema

所有配表为 UTF-8 带 BOM 的 CSV，首行表头，`#` 开头为注释行。
加载器：`src/game/tables.*`，启动时一次性读入内存结构体；解析失败 → Fatal 日志并退出。

## data/tables/player.csv

```
key,value,comment
max_hp,100,
move_speed,220,
jump_vel,-560,
gravity,1600,
roll_dist,130,像素
roll_iframe,0.35,秒
roll_cd,0.9,
hurt_iframe,0.8,
combo_window,0.45,
potions_start,1,
```

## data/tables/weapons.csv

```
id,name_key,dmg_mult,speed_mult,range_bonus,knock_mult,frames（连段伤害;分隔）
sword_prison,wpn_sword,1.0,1.0,0,1.0,12;14;22
sword_heavy,wpn_heavy,1.7,0.7,14,1.5,20;24;38
dagger_swift,wpn_dagger,0.65,1.45,-6,0.8,8;10;15
```

## data/tables/enemies.csv

```
id,name_key,hp,dmg,speed,score_coins,soul,anim_start,anim_count,scale,ai
slime,en_slime,30,12,70,3,1,0,2,3,chase
bat,en_bat,20,10,150,4,1,24,2,3,swoop
wraith,en_wraith,26,14,60,4,2,2,2,3,float
cyclops,en_cyclops,70,18,45,8,3,4,2,3,leap
bomber,en_bomber,16,25,90,6,2,15,3,3,explode
boss_golem,en_boss,600,20,110,60,50,21,3,4,boss
```

`anim_start/anim_count` 引用 pixel-platformer 角色图集（9×3，24px/帧）行主序帧号；scale 为整数放大。

## data/tables/upgrades.csv

```
id,name_key,desc_key,max_level,cost_base,cost_growth,effect_key,effect_per_level
hp_up,up_hp,up_hp_d,5,20,1.6,max_hp,20
atk_up,up_atk,up_atk_d,5,30,1.7,dmg_mult,0.10
roll_cd,up_roll,up_roll_d,3,25,1.8,roll_cd_delta,-0.15
potion_up,up_potion,up_potion_d,2,40,2.0,potions,1
greed,up_greed,up_greed_d,3,25,1.8,coin_mult,0.25
```

花费 = cost_base × cost_growth^level（取整）。

## data/tables/levels.csv

```
level,name_key,rooms_min,rooms_max,enemy_pool(权重id:n),density,tint,bgm
1,lv_1,5,7,slime:5;bat:3,0.8,blue,cave
2,lv_2,5,7,slime:3;bat:3;wraith:3;bomber:2,1.0,green,amb
3,lv_3,4,5,slime:2;bat:2;wraith:3;bomber:2;cyclops:3,1.2,red,amb
```

density：每战斗房敌人基数乘数。

## data/tables/loot.csv

```
source,coin_min,coin_max,soul_chance,soul_n,potion_chance
slime,2,5,0.15,1,0.03
bat,3,6,0.15,1,0.03
wraith,3,6,0.25,2,0.05
cyclops,6,12,0.5,3,0.10
bomber,4,8,0.3,2,0.05
boss_golem,40,60,1.0,50,1.0
```

## 存档（非配表）

`%APPDATA%/EchoesOfTheAbyss/save.dat`：纯文本 KV（`key=value` 每行一条），
记录 soul 总量、各 upgrade 等级、统计（总局数/总击杀/最佳通关时间）。手写解析，不引库。
