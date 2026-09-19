"""Elite Force 1.20's bot item choice (stvoyHM.exe botlib 0x48ec70, 0x48efb0).

The probe compiles the shipping BotChooseLTGItemSTVEF and BotChooseNBGItemSTVEF
from be_ai_goal.cpp against stubbed AAS, weight and goal-stack services, and
checks retail's rules: which items are eligible, the random whole-number
weight the game's last argument selects, goals still being avoided, the
item-only goal flag, the avoid times, the nearby-goal time limit and the way
back to the long-term goal. A source check pins the import routing.
"""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile
import unittest

from compiler_support import compiler_target_args

ROOT = Path(__file__).resolve().parents[1]


def between(source, start, end):
    first = source.index(start)
    return source[first:source.index(end, first)]


def probe_source():
    goal = (ROOT / 'code/botlib/be_ai_goal.cpp').read_text(encoding='utf-8')
    parts = [
        between(goal, 'static constexpr int AVOID_MINIMUM_TIME', '//location in the map'),
        between(goal, 'enum gametype_t {', 'struct levelitem_t'),
        between(goal, 'struct levelitem_t', 'static const fielddef_t iteminfo_fields'),
        between(goal, 'struct itemconfig_t', 'static bot_goalstate_t *botgoalstates'),
    ]
    functions = between(goal, 'static qboolean BotSTVEFItemEligible(',
                        '} //end of the function BotChooseNBGItemSTVEF')
    functions += '} //end of the function BotChooseNBGItemSTVEF\n'
    return PRELUDE + '\n'.join(parts) + STUBS + functions + CHECKS


PRELUDE = r'''
#include "code/qcommon/q_shared.h"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <map>
#include <vector>
struct weightconfig_t;
static constexpr int MAX_STRINGFIELD = 80;
#include "code/botlib/be_ai_goal.h"
'''

STUBS = r'''
static levelitem_t *levelitems;
static itemconfig_t *itemconfig;
static int g_gametype;
static bot_goalstate_t state;
static weightconfig_t *const config = reinterpret_cast<weightconfig_t *>(&state);
static int weightIndex[8];
static std::map<int, float> fuzzy;          // weight by weight number
static std::map<int, int> travel;           // travel time to a goal area
static std::map<int, float> avoiding;       // avoid time left by item number
static std::vector<bot_goal_t> pushed;
static std::vector<std::pair<int, float>> avoided;
static int reachArea = 5, lastArea;
bot_goalstate_t *BotGoalStateFromHandle(int handle) { return handle == 1 ? &state : nullptr; }
int BotReachabilityArea(vec3_t, int) { return reachArea; }
int AAS_AreaReachability(int area) { return area > 0; }
float FuzzyWeightUndecided(int *, weightconfig_t *wc, int weightnum) {
	assert(wc == config); return fuzzy[weightnum];
}
float BotAvoidGoalTime(int, int number) { return avoiding.count(number) ? avoiding[number] : 0; }
int AAS_AreaTravelTimeToGoalArea(int from, vec3_t, int goalarea, int) {
	// Travel from the bot's area is looked up by goal; back from an item to
	// the long-term goal (area 99) by the item's area plus 1000.
	if (from == reachArea) return travel.count(goalarea) ? travel[goalarea] : 0;
	return travel.count(from + 1000) ? travel[from + 1000] : 0;
}
static void BotAddToAvoidGoals(bot_goalstate_t *gs, int number, float time) {
	assert(gs == &state); avoided.push_back({number, time});
}
void BotPushGoal(int goalstate, bot_goal_t *goal) { assert(goalstate == 1); pushed.push_back(*goal); }
'''

CHECKS = r'''
static iteminfo_t infos[4];
static itemconfig_t ic{4, infos};
static levelitem_t items[6];

static void Reset() {
	std::memset(&state, 0, sizeof(state));
	state.itemweightconfig = config;
	state.itemweightindex = weightIndex;
	std::memset(items, 0, sizeof(items));
	for (int i = 0; i < 4; ++i) {
		infos[i] = {};
		infos[i].number = i;
		infos[i].respawntime = 30;
		VectorSet(infos[i].mins, -8, -8, -8);
		VectorSet(infos[i].maxs, 8, 8, 8);
		weightIndex[i] = i;
	}
	fuzzy.clear(); travel.clear(); avoiding.clear(); pushed.clear(); avoided.clear();
	levelitems = nullptr; itemconfig = &ic; g_gametype = GT_FFA; reachArea = 5;
}
// Links items[0..count) as the level's item list, each in area 10 + i.
static void Level(int count) {
	for (int i = 0; i < count; ++i) {
		items[i].number = 100 + i;
		items[i].iteminfo = i % 4;
		items[i].goalareanum = 10 + i;
		items[i].entitynum = 200 + i;
		VectorSet(items[i].goalorigin, (float)i, 0, 0);
		items[i].next = i + 1 < count ? &items[i + 1] : nullptr;
	}
	levelitems = count ? &items[0] : nullptr;
}
static int Ltg(int random = 0) {
	vec3_t origin = {0, 0, 0}; int inventory[8] = {};
	return BotChooseLTGItemSTVEF(1, origin, inventory, 0, random);
}
static int Nbg(bot_goal_t *ltg, float maxtime, int random = 0) {
	vec3_t origin = {0, 0, 0}; int inventory[8] = {};
	return BotChooseNBGItemSTVEF(1, origin, inventory, 0, ltg, maxtime, random);
}

int main() {
	// The best weight per unit of travel wins; the goal is an item goal only.
	Reset(); Level(3);
	fuzzy[0] = 10; fuzzy[1] = 30; fuzzy[2] = 20;
	travel[10] = 100; travel[11] = 400; travel[12] = 100;
	assert(Ltg() && pushed.size() == 1);
	assert(pushed[0].number == 102 && pushed[0].areanum == 12 && pushed[0].entitynum == 202);
	assert(pushed[0].flags == GFL_ITEM && pushed[0].iteminfo == 2 && pushed[0].origin[0] == 2);
	assert(pushed[0].mins[0] == -8 && pushed[0].maxs[2] == 8);
	// Half the respawn time, 30 seconds when that is under 10, 5 when dropped.
	assert(avoided.size() == 1 && avoided[0].first == 102 && avoided[0].second == 15);
	infos[2].respawntime = 18; avoided.clear(); pushed.clear();
	assert(Ltg() && avoided[0].second == 30);
	items[2].timeout = 4; fuzzy[2] = 20; avoided.clear(); pushed.clear();
	assert(Ltg() && pushed[0].number == 102 && avoided[0].second == 5);
	assert(pushed[0].flags == GFL_ITEM);   // no dropped flag

	// Any goal still being avoided is passed over, however far away.
	Reset(); Level(2); fuzzy[0] = 50; fuzzy[1] = 1; travel[10] = 100; travel[11] = 100;
	avoiding[100] = 0.001f;
	assert(Ltg() && pushed[0].number == 101);

	// Dropped items weigh 1000 more; roam points take their own scale.
	Reset(); Level(2); fuzzy[0] = 50; fuzzy[1] = 1; travel[10] = 100; travel[11] = 100;
	items[1].timeout = 9;
	assert(Ltg() && pushed[0].number == 101);
	Reset(); Level(2); fuzzy[0] = 10; fuzzy[1] = 10; travel[10] = 100; travel[11] = 100;
	items[1].flags = IFL_ROAM; items[1].weight = 3; items[1].entitynum = 0;
	assert(Ltg() && pushed[0].number == 101);

	// Eligibility: game type, bots, goal areas, and items that never spawned.
	Reset(); Level(1); fuzzy[0] = 5; travel[10] = 100;
	items[0].flags = IFL_NOTFREE; assert(!Ltg());
	g_gametype = GT_TEAM; assert(Ltg()); items[0].flags = IFL_NOTTEAM; pushed.clear(); assert(!Ltg());
	g_gametype = GT_SINGLE_PLAYER; assert(Ltg()); items[0].flags = IFL_NOTSINGLE; assert(!Ltg());
	items[0].flags = IFL_NOTBOT; assert(!Ltg());
	items[0].flags = 0; items[0].goalareanum = 0; assert(!Ltg());
	items[0].goalareanum = 10; items[0].entitynum = 0; assert(!Ltg());
	// The game's last argument admits it, and weighs it 1 to 10 at random.
	assert(Ltg(1));
	Reset(); Level(1); travel[10] = 100; fuzzy[0] = 0; items[0].entitynum = 0;
	int chosen = 0;
	for (int i = 0; i < 400; ++i) { avoided.clear(); pushed.clear(); chosen += Ltg(1); }
	assert(chosen > 350);    // weight 0 only when rand() & 0x7fff is 0
	fuzzy[0] = 0; items[0].entitynum = 200; assert(!Ltg());   // fuzzy weight 0 without it

	// No weight index, no area, no configuration: nothing is chosen.
	Reset(); Level(1); fuzzy[0] = 5; travel[10] = 100;
	weightIndex[0] = -1; assert(!Ltg()); weightIndex[0] = 0;
	reachArea = 0; state.lastreachabilityarea = 0; assert(!Ltg()); reachArea = 5;
	itemconfig = nullptr; assert(!Ltg()); itemconfig = &ic;
	travel[10] = 0; assert(!Ltg());

	// Nearby goals: within maxtime, and no longer back to the long-term goal.
	Reset(); Level(2); fuzzy[0] = 40; fuzzy[1] = 20; travel[10] = 100; travel[11] = 100;
	bot_goal_t ltg{}; ltg.areanum = 99; travel[99] = 300;
	travel[1010] = 500; travel[1011] = 200;   // back from each item to area 99
	assert(Nbg(&ltg, 150) && pushed[0].number == 101);
	pushed.clear(); avoided.clear();
	assert(!Nbg(&ltg, 100));                   // travel must be under maxtime
	items[0].timeout = 3; pushed.clear();       // a dropped item ignores the way back
	assert(Nbg(&ltg, 150) && pushed[0].number == 100 && avoided.back().second == 5);
	Reset(); Level(1); fuzzy[0] = 5; travel[10] = 100;
	assert(Nbg(nullptr, 150) && pushed[0].flags == GFL_ITEM);
	std::puts("ok");
	return 0;
}
'''


class STVEFBotItemChoiceTests(unittest.TestCase):
    def test_retail_item_choice_rules(self):
        compiler = shutil.which('clang++') or shutil.which('g++')
        if not compiler:
            self.skipTest('C++17 compiler unavailable')
        (ROOT / '.tmp').mkdir(exist_ok=True)
        with tempfile.TemporaryDirectory(prefix='stvef-items-', dir=ROOT / '.tmp') as temp:
            folder = Path(temp)
            source = folder / 'probe.cpp'
            executable = folder / ('probe.exe' if os.name == 'nt' else 'probe')
            source.write_text(probe_source(), encoding='utf-8')
            target = compiler_target_args(compiler)
            if os.name == 'nt' and not target:
                target = ['-DFNTECH3_Q3_ONLY']
            elif os.name != 'nt':
                target = ['-m32']
            build = subprocess.run([compiler, *target, '-std=c++17', '-D_CRT_SECURE_NO_WARNINGS',
                '-I', str(ROOT), str(source), '-o', str(executable)],
                cwd=ROOT, capture_output=True, text=True, timeout=120)
            self.assertEqual(build.returncode, 0, build.stdout + build.stderr)
            options = {'creationflags': subprocess.CREATE_NO_WINDOW} if os.name == 'nt' else {}
            run = subprocess.run([str(executable)], cwd=ROOT, capture_output=True, text=True, timeout=30, **options)
            self.assertEqual(run.returncode, 0, run.stdout + run.stderr)
            self.assertIn('ok', run.stdout)

    def test_the_game_imports_pass_the_last_argument(self):
        game = (ROOT / 'code/server/server_stvef_game.hpp').read_text(encoding='utf-8')
        ltg = between(game, 'case Call::BOTLIB_AI_CHOOSE_LTG_ITEM:', 'return true;')
        self.assertIn('BotChooseLTGItemSTVEF(', ltg)
        self.assertIn('static_cast<int>( args[5] )', ltg)
        nbg = between(game, 'case Call::BOTLIB_AI_CHOOSE_NBG_ITEM:', 'return true;')
        self.assertIn('BotChooseNBGItemSTVEF(', nbg)
        self.assertIn('SV_STVEFFloat( args[6] ), static_cast<int>( args[7] )', nbg)


if __name__ == '__main__':
    unittest.main()
