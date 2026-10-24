#include "ui/CodexPanel.h"

#include "audio/AudioSystem.h"
#include "core/App.h"
#include "core/Format.h"
#include "game/EnemyCatalog.h"
#include "game/Profile.h"
#include "ui/Widgets.h"
#include "view/Palette.h"
#include "view/Shapes.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace gb {

namespace {

struct EnemyEntry {
    EnemyKind kind;
    int sector; // guardians only (1-based)
    const char* description;
};

// Codex order: regular hostiles roughly in the order they first appear, then
// the five sector guardians. Sub-parts (minis, tail segments, mines,
// turrets) are folded into their parent's entry.
constexpr std::array<EnemyEntry, 17> kEnemyEntries{{
    {EnemyKind::Drifter, 0,
     "Slow homing diamond. Harmless alone, lethal in a crowd. Keep moving and let your guns do the work."},
    {EnemyKind::Dart, 0, "Locks on, flashes, then lunges in a straight line. Sidestep the moment it flashes."},
    {EnemyKind::Pinwheel, 0, "Erratic wanderer that ricochets off the arena walls. Hard to predict, easy to break."},
    {EnemyKind::Splitter, 0, "Bursts into a spray of fast minis when destroyed. Pop it at range, never point-blank."},
    {EnemyKind::Gunner, 0, "Keeps its distance and fires aimed shots. Weave between volleys and close the gap."},
    {EnemyKind::Swarmer, 0, "Tiny flocking chaser that arrives in clouds. Area damage and orbiting blades shred them."},
    {EnemyKind::Hive, 0,
     "Slow armoured carrier that keeps releasing swarmers. Destroy it first or drown in its brood."},
    {EnemyKind::Singularity, 0, "Gravity well that drags your ship and your bullets toward it. Dash against the pull."},
    {EnemyKind::Weaver, 0,
     "Serpent head trailing an armoured tail that soaks up shots. Destroy the head and the tail detonates."},
    {EnemyKind::Shielder, 0,
     "Its frontal shield deflects every shot that hits the front. Circle around and strike from behind."},
    {EnemyKind::Lancer, 0, "Stops to charge, then fires a laser along its aim line. Get off the line before it fires."},
    {EnemyKind::Bomber, 0, "Crosses the arena seeding mines in its wake. Clear a path or steer around the field."},
    {EnemyKind::BossWarden, 1,
     "A rotating fortress. Bullet rings and aimed volleys give way to summons, spirals and a telegraphed ram."},
    {EnemyKind::BossHydra, 2,
     "A colossal serpent whose body is a wall of bullets. Where the head turns, the tail follows."},
    {EnemyKind::BossSingularity, 3,
     "A gravity well that drags you into its spiralling arms. Fight the pull and strike between waves."},
    {EnemyKind::BossArchitect, 4,
     "A teleporting laser specialist that builds turrets. Tear down its constructions before they box you in."},
    {EnemyKind::BossCore, 5, "Shielded by orbiting nodes until they fall. Then it unleashes everything at once."},
}};

constexpr float kHeaderHeight = 110.0f;

const EnemyEntry& EntryFor(EnemyKind kind) {
    for (const EnemyEntry& entry : kEnemyEntries) {
        if (entry.kind == kind) {
            return entry;
        }
    }
    return kEnemyEntries.front();
}

// How far a body's outline reaches beyond its collision radius (gravity
// wells draw orbiting rings).
float VisualExtent(EnemyKind kind) {
    switch (kind) {
    case EnemyKind::Singularity: return 1.6f;
    case EnemyKind::BossSingularity: return 2.3f;
    default: return 1.0f;
    }
}

// Enemy previews are sized by their collision radius; scale them so their
// outline fills a given radius regardless of their in-game size.
void DrawEnemyFitted(NeonRenderer& neon, EnemyKind kind, Vector2 center, float radius, float time) {
    shapes::DrawEnemyPreview(neon, kind, center, radius / (GetEnemyInfo(kind).radius * VisualExtent(kind)), time);
}

} // namespace

void CodexPanel::Open(App& app) {
    if (enemies_.empty()) {
        for (const EnemyEntry& entry : kEnemyEntries) {
            enemies_.push_back(entry.kind);
        }
        for (const UpgradeInfo& info : AllUpgrades()) {
            upgrades_.push_back(info.id);
        }
    }
    active_ = true;
    tab_ = Tab::Hostiles;
    selected_ = 0;
    selectPulse_ = 1.0f;
    app.In().ConsumeAll();
}

int CodexPanel::EntryCount() const {
    return static_cast<int>(tab_ == Tab::Hostiles ? enemies_.size() : upgrades_.size());
}

bool CodexPanel::Discovered(const App& app, int index) const {
    const Profile& profile = app.Progress();
    const auto i = static_cast<std::size_t>(index);
    return tab_ == Tab::Hostiles ? profile.EnemyDiscovered(enemies_[i]) : profile.UpgradeDiscovered(upgrades_[i]);
}

CodexPanel::Layout CodexPanel::ComputeLayout(const App& app) const {
    const Vector2 ui = app.Gfx().UiSize();
    Layout l;
    const float width = std::min(1640.0f, ui.x - 80.0f);
    const float height = std::min(840.0f, ui.y - 120.0f);
    l.panel = {ui.x * 0.5f - width * 0.5f, ui.y * 0.5f - height * 0.5f, width, height};
    l.columns = tab_ == Tab::Hostiles ? 6 : 8;
    l.gap = 12.0f;
    const int rows = (EntryCount() + l.columns - 1) / l.columns;
    const float gridWidth = width * 0.56f;
    const float gridHeight = height - kHeaderHeight - 80.0f;
    const float byWidth = (gridWidth - l.gap * static_cast<float>(l.columns - 1)) / static_cast<float>(l.columns);
    const float byHeight = (gridHeight - l.gap * static_cast<float>(rows - 1)) / static_cast<float>(rows);
    l.tile = std::min({140.0f, byWidth, byHeight});
    l.gridOrigin = {l.panel.x + 40.0f, l.panel.y + kHeaderHeight};
    const float detailX = l.gridOrigin.x + gridWidth + 30.0f;
    l.detail = {detailX, l.panel.y + kHeaderHeight, l.panel.x + width - 40.0f - detailX, gridHeight};
    return l;
}

Rectangle CodexPanel::TabRect(const Layout& layout, Tab tab) const {
    const float x = layout.panel.x + 250.0f + (tab == Tab::Hostiles ? 0.0f : 320.0f);
    return {x, layout.panel.y + 26.0f, 300.0f, 50.0f};
}

Rectangle CodexPanel::TileRect(const Layout& layout, int index) const {
    const int col = index % layout.columns;
    const int row = index / layout.columns;
    return {layout.gridOrigin.x + static_cast<float>(col) * (layout.tile + layout.gap),
            layout.gridOrigin.y + static_cast<float>(row) * (layout.tile + layout.gap), layout.tile, layout.tile};
}

void CodexPanel::SwitchTab(App& app, Tab tab) {
    if (tab == tab_) {
        return;
    }
    tab_ = tab;
    selected_ = 0;
    selectPulse_ = 1.0f;
    app.Audio().Play(SfxId::UiConfirm, 0.6f);
}

bool CodexPanel::Update(App& app, float dt) {
    if (!active_) {
        return false;
    }
    time_ += dt;
    selectPulse_ = std::max(0.0f, selectPulse_ - dt * 4.0f);
    const Input& in = app.In();

    if (in.Pressed(Action::TabPrev) || in.Pressed(Action::TabNext)) {
        SwitchTab(app, tab_ == Tab::Hostiles ? Tab::Upgrades : Tab::Hostiles);
    }

    const Layout layout = ComputeLayout(app);
    const int count = EntryCount();
    int next = selected_;
    if (in.Repeated(Action::Left) && next % layout.columns > 0) {
        --next;
    }
    if (in.Repeated(Action::Right) && next % layout.columns < layout.columns - 1 && next + 1 < count) {
        ++next;
    }
    if (in.Repeated(Action::Up) && next - layout.columns >= 0) {
        next -= layout.columns;
    }
    if (in.Repeated(Action::Down)) {
        // Dropping into a partial last row lands on its final entry.
        const int lastRow = (count - 1) / layout.columns;
        if (next / layout.columns < lastRow) {
            next = std::min(next + layout.columns, count - 1);
        }
    }

    if (in.MouseMoved() || in.MouseClicked()) {
        for (int i = 0; i < count; ++i) {
            if (ui::MouseOver(app, TileRect(layout, i))) {
                next = i;
                break;
            }
        }
    }
    if (in.MouseClicked()) {
        for (Tab tab : {Tab::Hostiles, Tab::Upgrades}) {
            if (ui::MouseOver(app, TabRect(layout, tab))) {
                SwitchTab(app, tab);
                return false;
            }
        }
    }
    if (next != selected_) {
        selected_ = next;
        selectPulse_ = 1.0f;
        app.Audio().Play(SfxId::UiMove, 0.5f);
    }

    if (in.Pressed(Action::Back) || in.Pressed(Action::Pause)) {
        app.Audio().Play(SfxId::UiBack, 0.7f);
        app.In().ConsumeAll();
        active_ = false;
        return true;
    }
    return false;
}

void CodexPanel::Draw(App& app) {
    if (!active_) {
        return;
    }
    const Layout layout = ComputeLayout(app);
    const Profile& profile = app.Progress();
    TextRenderer& text = app.Gfx().Text();
    NeonRenderer& neon = app.Gfx().Neon();
    const int count = EntryCount();

    ui::Dim(app, 0.6f);
    ui::Panel(app, layout.panel, palette::kUiAccent);
    text.Draw(FontId::Heading, "CODEX", {layout.panel.x + 40.0f, layout.panel.y + 30.0f}, 34.0f, WHITE, Align::Left,
              0.15f);

    // Tabs with decode progress.
    int knownEnemies = 0;
    for (EnemyKind kind : enemies_) {
        knownEnemies += profile.EnemyDiscovered(kind) ? 1 : 0;
    }
    int knownUpgrades = 0;
    for (UpgradeId id : upgrades_) {
        knownUpgrades += profile.UpgradeDiscovered(id) ? 1 : 0;
    }
    for (Tab tab : {Tab::Hostiles, Tab::Upgrades}) {
        const Rectangle r = TabRect(layout, tab);
        const bool current = tab == tab_;
        DrawRectangleRec(r, current ? WithAlpha(palette::kUiAccent, 0.16f) : WithAlpha(palette::kUiPanelLight, 0.6f));
        DrawRectangleRec({r.x, r.y + r.height - 3.0f, r.width, 3.0f},
                         current ? palette::kUiAccent : WithAlpha(palette::kUiBorder, 0.7f));
        const bool hostiles = tab == Tab::Hostiles;
        const char* label = hostiles ? "HOSTILES" : "UPGRADES";
        const int known = hostiles ? knownEnemies : knownUpgrades;
        const int total = static_cast<int>(hostiles ? enemies_.size() : upgrades_.size());
        text.Draw(FontId::Heading, label, {r.x + 18.0f, r.y + 13.0f}, 21.0f, current ? WHITE : palette::kUiDim,
                  Align::Left, 0.12f);
        text.Draw(FontId::BodyBold, TextFormat("%d / %d", known, total), {r.x + r.width - 16.0f, r.y + 10.0f}, 26.0f,
                  current ? palette::kUiAccent : palette::kUiFaint, Align::Right);
    }

    // Tile backgrounds.
    for (int i = 0; i < count; ++i) {
        const Rectangle r = TileRect(layout, i);
        const bool known = Discovered(app, i);
        DrawRectangleRec(r, known ? WithAlpha(palette::kUiPanelLight, 0.9f) : WithAlpha(palette::kUiPanel, 0.8f));
    }

    // Tile frames and icons.
    neon.Begin();
    for (int i = 0; i < count; ++i) {
        const Rectangle r = TileRect(layout, i);
        const Vector2 c{r.x + r.width * 0.5f, r.y + r.height * 0.5f};
        const bool known = Discovered(app, i);
        const bool selected = i == selected_;
        Color accent = palette::kUiBorder;
        if (known) {
            accent = tab_ == Tab::Hostiles ? GetEnemyInfo(enemies_[static_cast<std::size_t>(i)]).color
                                           : RarityColor(GetUpgradeInfo(upgrades_[static_cast<std::size_t>(i)]).rarity);
        }
        const std::array<Vector2, 4> corners{Vector2{r.x, r.y}, Vector2{r.x + r.width, r.y},
                                             Vector2{r.x + r.width, r.y + r.height}, Vector2{r.x, r.y + r.height}};
        const Color frame = selected ? palette::kUiAccent : WithAlpha(accent, known ? 0.55f : 0.35f);
        neon.Polyline(corners, true, selected ? 3.0f + 2.0f * selectPulse_ : 1.5f, frame);
        if (selected) {
            neon.Glow(c, r.width * 0.75f, WithAlpha(palette::kUiAccent, 0.1f));
        }
        if (!known) {
            continue;
        }
        if (tab_ == Tab::Hostiles) {
            const EnemyKind kind = enemies_[static_cast<std::size_t>(i)];
            DrawEnemyFitted(neon, kind, c, r.width * (IsBoss(kind) ? 0.3f : 0.24f), time_);
        } else {
            shapes::DrawUpgradeIcon(neon, upgrades_[static_cast<std::size_t>(i)], c, r.width * 0.62f,
                                    LerpColor(accent, WHITE, 0.2f), time_);
        }
    }
    neon.End();
    for (int i = 0; i < count; ++i) {
        if (!Discovered(app, i)) {
            const Rectangle r = TileRect(layout, i);
            text.Draw(FontId::Heading, "?", {r.x + r.width * 0.5f, r.y + r.height * 0.5f - 16.0f}, 30.0f,
                      WithAlpha(palette::kUiFaint, 0.9f), Align::Center);
        }
    }

    // Detail pane for the selection.
    const Rectangle d = layout.detail;
    DrawRectangleRec(d, WithAlpha(palette::kUiPanel, 0.7f));
    DrawRectangleRec({d.x, d.y, 2.0f, d.height}, WithAlpha(palette::kUiBorder, 0.6f));
    const bool known = Discovered(app, selected_);
    if (tab_ == Tab::Hostiles) {
        DrawEnemyDetail(app, layout, enemies_[static_cast<std::size_t>(selected_)], known);
    } else {
        DrawUpgradeDetail(app, layout, upgrades_[static_cast<std::size_t>(selected_)], known);
    }

    const float promptY = layout.panel.y + layout.panel.height - 46.0f;
    ui::Prompt(app, {layout.panel.x + 40.0f, promptY}, "Q / E", "LB / RB", "SWITCH TAB", Align::Left, 22.0f);
    ui::Prompt(app, {layout.panel.x + layout.panel.width - 40.0f, promptY}, "ESC", "B", "BACK", Align::Right, 22.0f);
}

void CodexPanel::DrawEnemyDetail(App& app, const Layout& layout, EnemyKind kind, bool known) {
    TextRenderer& text = app.Gfx().Text();
    NeonRenderer& neon = app.Gfx().Neon();
    const Rectangle d = layout.detail;
    const EnemyInfo& info = GetEnemyInfo(kind);
    const EnemyEntry& entry = EntryFor(kind);
    const bool boss = IsBoss(kind);
    const Vector2 center{d.x + d.width * 0.5f, d.y + 150.0f};
    const Color color = known ? info.color : palette::kUiFaint;

    neon.Begin();
    neon.RegularPolygon(center, 6, 120.0f, kPi / 6.0f, 2.0f, WithAlpha(color, 0.5f));
    neon.Glow(center, 150.0f, WithAlpha(color, known ? 0.14f : 0.05f));
    if (known) {
        DrawEnemyFitted(neon, kind, center, boss ? 80.0f : 56.0f, time_);
    }
    neon.End();

    float y = d.y + 300.0f;
    if (!known) {
        text.Draw(FontId::Heading, "UNKNOWN SIGNAL", {center.x, y}, 32.0f, palette::kUiDim, Align::Center, 0.1f);
        text.DrawWrapped(FontId::Body, "Encounter this hostile in a run to decode its data.", {d.x + 40.0f, y + 70.0f},
                         d.width - 80.0f, 26.0f, palette::kUiFaint, 1.12f, Align::Center);
        return;
    }
    text.DrawGlow(FontId::Heading, ToUpper(info.name), {center.x, y}, 34.0f, WHITE, info.color, Align::Center, 0.08f);
    y += 50.0f;
    const char* role = boss ? TextFormat("SECTOR %02d GUARDIAN", entry.sector) : "HOSTILE";
    text.Draw(FontId::Heading, role, {center.x, y}, 18.0f, info.color, Align::Center, 0.25f);
    y += 46.0f;
    y += text.DrawWrapped(FontId::Body, entry.description, {d.x + 40.0f, y}, d.width - 80.0f, 27.0f, palette::kUiText,
                          1.12f, Align::Center);
    if (boss) {
        return;
    }
    // Base stats (before sector and difficulty scaling).
    struct Stat {
        const char* label;
        int value;
    };
    const std::array<Stat, 3> stats{{
        {"INTEGRITY", static_cast<int>(info.hp)},
        {"SPEED", static_cast<int>(info.speed)},
        {"SCORE", info.score},
    }};
    const float columnWidth = (d.width - 80.0f) / static_cast<float>(stats.size());
    const float statY = d.y + d.height - 90.0f;
    for (std::size_t i = 0; i < stats.size(); ++i) {
        const float x = d.x + 40.0f + columnWidth * (static_cast<float>(i) + 0.5f);
        text.Draw(FontId::BodyBold, TextFormat("%d", stats[i].value), {x, statY}, 34.0f, palette::kUiText,
                  Align::Center);
        text.Draw(FontId::Heading, stats[i].label, {x, statY + 44.0f}, 15.0f, palette::kUiDim, Align::Center, 0.2f);
    }
}

void CodexPanel::DrawUpgradeDetail(App& app, const Layout& layout, UpgradeId id, bool known) {
    TextRenderer& text = app.Gfx().Text();
    NeonRenderer& neon = app.Gfx().Neon();
    const Profile& profile = app.Progress();
    const Rectangle d = layout.detail;
    const UpgradeInfo& info = GetUpgradeInfo(id);
    const Color rarity = RarityColor(info.rarity);
    const Vector2 center{d.x + d.width * 0.5f, d.y + 150.0f};
    const Color color = known ? rarity : palette::kUiFaint;

    neon.Begin();
    neon.RegularPolygon(center, 6, 120.0f, kPi / 6.0f, 2.0f, WithAlpha(color, 0.5f));
    neon.Glow(center, 150.0f, WithAlpha(color, known ? 0.14f : 0.05f));
    if (known) {
        shapes::DrawUpgradeIcon(neon, id, center, 150.0f, LerpColor(rarity, WHITE, 0.2f), time_);
    }
    neon.End();

    float y = d.y + 300.0f;
    const auto recipeOf = [](UpgradeId evolution) -> const EvolutionRecipe* {
        for (const EvolutionRecipe& recipe : AllEvolutions()) {
            if (recipe.evolution == evolution) {
                return &recipe;
            }
        }
        return nullptr;
    };
    if (!known) {
        text.Draw(FontId::Heading, "ENCRYPTED", {center.x, y}, 32.0f, palette::kUiDim, Align::Center, 0.1f);
        const char* hint = "Draft this upgrade in a run to decode its data.";
        if (const EvolutionRecipe* recipe = recipeOf(id); recipe != nullptr) {
            const bool partsKnown =
                profile.UpgradeDiscovered(recipe->module) && profile.UpgradeDiscovered(recipe->catalyst);
            hint = partsKnown ? TextFormat("Max out %s while holding %s to evolve it.",
                                           GetUpgradeInfo(recipe->module).name, GetUpgradeInfo(recipe->catalyst).name)
                              : "An evolution. Max out a module while holding its catalyst.";
        }
        text.DrawWrapped(FontId::Body, hint, {d.x + 40.0f, y + 70.0f}, d.width - 80.0f, 26.0f, palette::kUiFaint, 1.12f,
                         Align::Center);
        return;
    }
    text.DrawGlow(FontId::Heading, info.name, {center.x, y}, 34.0f, WHITE, rarity, Align::Center, 0.05f);
    y += 50.0f;
    text.Draw(FontId::Heading, TextFormat("%s  //  %s", RarityName(info.rarity), CategoryName(info.category)),
              {center.x, y}, 18.0f, rarity, Align::Center, 0.25f);
    y += 46.0f;
    y += text.DrawWrapped(FontId::Body, info.description, {d.x + 40.0f, y}, d.width - 80.0f, 27.0f, palette::kUiText,
                          1.12f, Align::Center);

    const char* footer = nullptr;
    Color footerColor = palette::kUiDim;
    if (const EvolutionRecipe* recipe = recipeOf(id); recipe != nullptr) {
        footer =
            TextFormat("%s (LV 5)  +  %s", GetUpgradeInfo(recipe->module).name, GetUpgradeInfo(recipe->catalyst).name);
        footerColor = palette::kElite;
    } else if (const EvolutionRecipe* evolves = EvolutionForModule(id); evolves != nullptr) {
        footer = TextFormat("EVOLVES WITH %s", GetUpgradeInfo(evolves->catalyst).name);
        footerColor = palette::kElite;
    } else if (info.maxStacks > 1) {
        footer = TextFormat("STACKS UP TO %d TIMES", info.maxStacks);
    } else if (info.maxStacks == 0) {
        footer = id == UpgradeId::Overcharge ? "STACKS WITHOUT LIMIT" : "INSTANT EFFECT";
    } else {
        footer = "UNIQUE";
    }
    text.Draw(FontId::BodyBold, footer, {center.x, d.y + d.height - 60.0f}, 24.0f, footerColor, Align::Center, 0.06f);
}

} // namespace gb
