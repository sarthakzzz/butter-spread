# Butter Spread: felt breakfast table art

`scene/spread-scene.html` renders every layer (a three.js needle-felt scene).
Copy `/System/Library/Fonts/Supplemental/Georgia Bold.ttf` to `scene/fonts/georgia-bold.ttf` (not committed). Run `python3 scene/serve.py`, open `http://localhost:8767/spread-scene.html?only=bg|title|props|butter`,
then copy `scene/out/*` (minus test/json files) into `Resources/art/`.
`butter` also writes `toast.json` (knife tip positions used in Source/PluginEditor.cpp).
