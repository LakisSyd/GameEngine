# Offline pack: с чего начать

Ты уезжаешь без интернета, поэтому вот короткий вход в пакет документов.

## Читать в таком порядке

1. `DX_FRAMEWORK_ENGINE_TEXTBOOK_RU.md` или `DX_FRAMEWORK_ENGINE_TEXTBOOK_RU.docx`

   Главный учебник по актуальному устройству движка: ownership, кадр, ECS, математика, DirectX 11, Bullet, симуляция, NPC и практические задания. Главы 44–50 специально объясняют, как читать собственный код и самостоятельно довести дружественного NPC и создать преследующего врага.

2. `DX_FRAMEWORK_FULL_DOCUMENTATION_RU.md` или `DX_FRAMEWORK_FULL_DOCUMENTATION_RU.docx`

   Главный полный справочник по текущему проекту: сборка, C++, ECS, кадр, DirectX 11, Bullet, капсульный контроллер, игрок, NPC, NavMesh, энергетика, атмосфера, повреждения, ремонт, корабль, сохранения и диагностика. Если после перерыва всё забылось — начинай отсюда.

3. `ROADMAP_14_DAYS_OFFLINE_STUDY_RU.md` или `ROADMAP_14_DAYS_OFFLINE_STUDY_RU.docx`

   Новый двухнедельный учебно-производственный план. Он ведёт к автономной аварии, которую NPC замечают и устраняют без привязки к игроку. На каждый день есть теория, код, задачи, тесты и критерий готовности.

4. `CODE_METHOD_MAP.md`

   Подробный индекс существующих файлов и методов. Открывай его каждый раз перед тем, как писать новый метод.

5. `CPP_ENGINE_THEORY_OFFLINE.md`

   Расширенная теория C++ и движка: ECS, указатели, ссылки, move, DirectXMath, Bullet, render pipeline, events.

6. `DEV_NOTES_OFFLINE.md`

   Твои рабочие заметки. Заполняй после каждого блока.

## Старые планы

Они оставлены как дополнительный материал, но новый основной маршрут находится в `ROADMAP_14_DAYS_OFFLINE_STUDY_RU.md`.

- `OFFLINE_ROADMAP_WEEKEND.md`;
- `ROADMAP_14_DAYS_SPACE_SIM_RU.md`;
- `ROADMAP_7_DAYS_AUTONOMOUS_SPACE_GAME_RU.md`.

## Мини-цикл работы

```text
прочитал задачу
нашел код через rg
сделал маленькое изменение
собрал Debug x64
проверил в движке
записал результат в DEV_NOTES_OFFLINE.md
```

## Самые важные команды

```powershell
rg -n "ApplicationClass::Frame" src
rg -n "ResolveDoorRoomLinks|DoorSystem|DoorComponent" .
rg -n "RoomComponent|RoomEnvironmentSystem|oxygen" .
rg -n "HullPanelComponent|HullBreachComponent|MeshCuttingTest" .
rg -n "SpawnObjectDesc|SpawnObject\\(" src
```

Сборка:

```powershell
& 'C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\amd64\MSBuild.exe' 'C:\Users\Lakis\Desktop\DX_FrameWork\DX_FrameWork.sln' /m /p:Configuration=Debug /p:Platform=x64 /v:minimal
```

## Главный принцип

Не пытайся сделать много новых систем сразу.

Сделай один стабильный кусок:

```text
две комнаты
одна дверь
один игрок
один interact
одна утечка кислорода
один понятный HUD/debug
```

Это уже будет настоящая основа игры.
