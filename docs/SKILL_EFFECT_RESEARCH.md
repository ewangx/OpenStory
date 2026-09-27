# Finding a skill's animation in the NX data

How to answer "what should skill X look like?" when the client shows nothing
(or the wrong thing). Worked example: Hermit Flash Jump (`4111006`).

## Background: how skills get visuals

`Gameplay/Combat/Skill.cpp` builds each skill's presentation from `Skill.nx`:

| Piece | Node(s) under `Skill/<job>.img/skill/<id>/` | Class when missing |
|---|---|---|
| Cast effect on character | `effect`, `effect0` (multi), `CharLevel/*/effect` (by level) | `NoUseEffect` |
| Character pose | `action`, `action/0`, `action/1`, `level/*/action` | `NoAction` (unless `SkillData::is_attack()`, then weapon-swing fallback) |
| Projectile | `ball`, `level/*/ball` | `RegularBullet`, `projectile = false` |
| Hit effect on mob | `hit`, `level/*/hit` | `NoHitEffect` |
| Sound | `Sound.nx: Skill.img/<id>/Use|Hit` | silent missing node |

Movement skills (Teleport, Flash Jump) have **no** `effect`/`action` nodes in v83
data, so the generic path renders movement + sound with no visual. The client
must substitute an effect explicitly — the established pattern is Teleport's
`BasicEff/Teleport` puff, see `Combat::apply_teleport` and
`OtherChar::send_movement`.

## Step 1 — check whether the data exists at all

Quick check in the Cosmic XML mirror:

```sh
grep -A 60 '"<skillid>"' Cosmic/wz/Skill.wz/<job>.img.xml | head -80
```

List a skill's direct children to see which pieces are authored:

```sh
python3 -c "
import xml.etree.ElementTree as ET
tree = ET.parse('Cosmic/wz/Skill.wz/<job>.img.xml')
for imgdir in tree.getroot().iter('imgdir'):
    if imgdir.get('name') == '<skillid>':
        print([c.get('name') for c in imgdir])
"
```

Skill names (to confirm you have the right id):

```sh
python3 -c "
import xml.etree.ElementTree as ET
tree = ET.parse('Cosmic/wz/String.wz/Skill.img.xml')
for imgdir in tree.getroot().iter('imgdir'):
    if (imgdir.get('name') or '').startswith('<jobprefix>'):
        for c in imgdir:
            if c.tag == 'string' and c.get('name') == 'name':
                print(imgdir.get('name'), '-', c.get('value'))
"
```

**Always confirm against the real NX** (`maplestory-v83-assets/nx/Skill.nx`);
the Cosmic XML can be stale. Absence is proven only if the node is missing in
the NX too — use the extractor in Step 3 (a `MISS` line is the proof).

## Step 2 — survey candidate effects in `Effect.nx`

List one-shot candidates:

```sh
python3 -c "
import re
with open('Cosmic/wz/Effect.wz/BasicEff.img.xml') as f:
    content = f.read()
for t in re.findall(r'^  <imgdir name=\"([^\"]+)\"', content, re.M):
    print(t)
"
```

Then check which candidates are **code-only** (referenced by no data file —
same pattern as Teleport, which only the client code names):

```sh
grep -rln "BasicEff.img/<Name>" Cosmic/wz/ | head
```

No hits means the node is only reachable from hardcoded client code, which
makes it safe to repurpose and a plausible slot for a hardcoded skill visual.
Also pull frame metadata (count, size, origin, per-frame delay ms, `z`):

```sh
python3 -c "
import xml.etree.ElementTree as ET
tree = ET.parse('Cosmic/wz/Effect.wz/BasicEff.img.xml')
for imgdir in tree.getroot().iter('imgdir'):
    if imgdir.get('name') == '<Name>':
        for c in imgdir:
            if c.tag == 'canvas':
                info = {'size': (c.get('width'), c.get('height'))}
                for p in c:
                    if p.tag == 'vector' and p.get('name') == 'origin':
                        info['origin'] = (p.get('x'), p.get('y'))
                    if p.tag == 'int':
                        info[p.get('name')] = p.get('value')
                print(' ', c.get('name'), info)
"
```

## Step 3 — look at the actual pixels

`read` renders PNGs, so dump candidate frames with this throwaway tool
(compiled against the repo's vendored NoLifeNx; needs no game init):

```sh
c++ -std=c++17 -O1 -o /tmp/nxdump /tmp/nxdump.cpp \
  OpenStory/vendor/NoLifeNx/nlnx/file.cpp \
  OpenStory/vendor/NoLifeNx/nlnx/node.cpp \
  OpenStory/vendor/NoLifeNx/nlnx/nx.cpp \
  OpenStory/vendor/NoLifeNx/nlnx/bitmap.cpp \
  OpenStory/vendor/NoLifeNx/nlnx/audio.cpp \
  OpenStory/vendor/NoLifeNx/nlnx/includes/lz4_v1_8_2_win64/lz4.c \
  -IOpenStory/vendor/NoLifeNx \
  -IOpenStory/vendor/NoLifeNx/nlnx/includes/lz4_v1_8_2_win64/include \
  -IOpenStory/vendor/stb
mkdir -p /tmp/fx
/tmp/nxdump maplestory-v83-assets/nx/Effect.nx /tmp/fx \
  "BasicEff.img/<Name>/0" "BasicEff.img/<Name>/1" ...
```

`nxdump.cpp` (NX bitmaps are BGRA — swap R/B for PNG):

```cpp
// Dump NX bitmap nodes to PNG for visual inspection.
#include <nlnx/file.hpp>
#include <nlnx/node.hpp>
#include <nlnx/bitmap.hpp>
#include <cstdio>
#include <string>
#include <vector>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

int main(int argc, char** argv) {
    if (argc < 3) {
        std::printf("usage: nxdump <nxfile> <outdir> <nodepath>...\n");
        return 1;
    }
    nl::file f(argv[1]);
    nl::node root = f.root();
    if (!root) {
        std::printf("open fail: %s\n", argv[1]);
        return 1;
    }
    std::string outdir = argv[2];
    for (int i = 3; i < argc; ++i) {
        std::string np = argv[i];
        nl::node n = root;
        size_t pos = 0;
        bool ok = true;
        while (pos <= np.size()) {
            size_t slash = np.find('/', pos);
            std::string part = (slash == std::string::npos)
                ? np.substr(pos) : np.substr(pos, slash - pos);
            if (!part.empty()) n = n[part];
            if (!n) { ok = false; break; }
            if (slash == std::string::npos) break;
            pos = slash + 1;
        }
        if (!ok || !n || n.data_type() != nl::node::type::bitmap) {
            std::printf("MISS %s\n", np.c_str());
            continue;
        }
        nl::bitmap bmp = n.get_bitmap();
        int w = bmp.width(), h = bmp.height();
        const unsigned char* src =
            static_cast<const unsigned char*>(bmp.data());
        std::vector<unsigned char> px(size_t(w) * h * 4);
        for (int p = 0; p < w * h; ++p) {
            px[p * 4 + 0] = src[p * 4 + 2];
            px[p * 4 + 1] = src[p * 4 + 1];
            px[p * 4 + 2] = src[p * 4 + 0];
            px[p * 4 + 3] = src[p * 4 + 3];
        }
        std::string safe = np;
        for (char& c : safe)
            if (c == '/' || c == '.') c = '_';
        std::string out = outdir + "/" + safe + ".png";
        if (stbi_write_png(out.c_str(), w, h, 4, px.data(), w * 4))
            std::printf("OK %s (%dx%d)\n", np.c_str(), w, h);
        else
            std::printf("WRITE FAIL %s\n", out.c_str());
    }
    return 0;
}
```

Judge candidates on: shape (ring/column/streak/smoke), color, size vs the
~100px character, whether a center glyph or directional trail survives
mirroring (`show_attack_effect` mirrors by facing), total duration
(sum of frame delays, scaled by the character's attack speed), and `z`.

## Step 4 — wire the substitution (both paths)

A skill visual needs **two** call sites, or half the cases stay silent:

1. **Local player** — `Combat::apply_*`: play the effect on `player` via
   `show_attack_effect(static Animation(...), 0)` (static to avoid reloading
   the NX node every cast) plus any stance fix.
2. **Remote players** — their casts arrive as movement, not skill packets.
   Detect the movement command byte in `OtherChar::send_movement`
   (Flash Jump = command `6`, see `Net/Handlers/Helpers/MovementParser.cpp`)
   and mirror the same visual. Teleport is the template: `type == TELEPORT`
   → puff; Flash Jump: `command == 6` → ring.

Two supporting pieces used by the Flash Jump fix:

- `Character/SkillId.h`: register every job's id for the skill (Hermit
  `4111006` + Night Walker `14101004`), with a shared
  `Combat::is_flash_jump_skill()` helper so future jobs plug in one place.
- `CharLook::restart_stance()`: `set_stance()` is intentionally a no-op for
  the already-active stance (so walk/fall loops keep animating). A mid-air
  re-kick needs a forced frame-0 restart — do **not** change `set_stance`
  globally or every looping animation stalls.

Verify: `cmake -S . -B build && cmake --build build --config Debug
--target OpenStory OpenStoryNetTests OpenStoryEquipJobTests &&
ctest --test-dir build -C Debug --output-on-failure`.

## Worked example: Flash Jump

- `4111006` children in both Cosmic XML and real `Skill.nx`: only
  `icon*`, `level` (`hs` + `mpCon`), `req`. No `effect`, no `action` →
  `NoUseEffect` + `NoAction` is correct loader behavior, not a bug.
- Reported expectation: blue ring around the character.
- Survey results: Teleport = blue column (wrong shape, already means mage
  warp); VerticalJump = white smoke (wrong color/shape); Assaulter/SoulRush
  = streaks; `BasicEff/Flying` = teal-blue ring + energy orbs + dash smoke,
  5 frames x 120ms, code-unreferenced, aerial-themed name. Only ring-shaped
  blue-ish candidate → substituted for local (`Combat::apply_flash_jump`)
  and remote (`OtherChar`, command 6) paths, plus JUMP re-kick.
- Confidence: best match in v83 assets, **not** a proven official mapping —
  v83 ships no dedicated Flash Jump effect, so any choice here is a
  substitution by construction.
