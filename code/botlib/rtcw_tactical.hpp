/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_RTCW_TACTICAL_HPP
#define FNTECH3_RTCW_TACTICAL_HPP
#include "../compat/rtcw_game_abi.h"

// Included beside the routing/visibility implementation. These types stay
// inside the engine; neither tactical scratch nor Quake POD crosses the ABI.
namespace fntech3::rtcw::tactical {

static float hideTime, attackTime;
static int hideWork;
static uint64_t nextToken;

static bool Position(const float *point) {
	return point && routing::Coordinate(point[0]) && routing::Coordinate(point[1])
		&& routing::Coordinate(point[2]);
}

static bool Entity(int number) {
	const auto &world = AAS_EntityWorld();
	return number >= 0 && number < (1 << FNTECH3_RTCW_GENTITYNUM_BITS)
		&& number < world.maxentities && world.entities;
}

static bool Area(int number) { return number > 0 && number < aasworld.numareas; }

static bool Ready() {
	return Com_GameIsRTCW() && aasworld.initialized && aasworld.loaded
		&& aasworld.numareas > 1 && aasworld.numareas <= 65535
		&& aasworld.numareasettings == aasworld.numareas && aasworld.areasettings
		&& aasworld.reachabilitysize >= 0 && aasworld.reachability
		&& aasworld.areacontentstravelflags && aasworld.areatraveltimes
		&& aasworld.clusterareacache && aasworld.portalcache
		&& aasworld.rtcwRouteCacheLoaded && aasworld.rtcwVisibility
		&& aasworld.rtcwDecodedVisibility && aasworld.rtcwWaypoints
		&& !aasworld.rtcwTacticalToken
		&& (!aasworld.rtcwTacticalNodes || aasworld.rtcwTacticalCapacity == aasworld.numareas);
}

static bool Link(const aas_link_t *link) {
	if (!link || !aasworld.linkheap || aasworld.linkheapsize <= 0) return false;
	const auto address = reinterpret_cast<uintptr_t>(link);
	const auto base = reinterpret_cast<uintptr_t>(aasworld.linkheap);
	return address >= base && (address - base) % sizeof(*link) == 0
		&& (address - base) / sizeof(*link) < static_cast<unsigned>(aasworld.linkheapsize);
}

static bool Occupied(int area, int ignore) {
	// Original SP explicitly disables this penalty for its multiple hulls.
	if (Com_GameIsRTCWSP()) return false;
	if (!aasworld.arealinkedentities) return false;
	const auto &entities = AAS_EntityWorld();
	int remaining = aasworld.linkheapsize;
	for (auto *link = aasworld.arealinkedentities[area]; link; link = link->next_ent) {
		// Broken/cyclic linkage cannot be treated as an unoccupied candidate.
		if (remaining <= 0 || !Link(link) || link->areanum != area || !Entity(link->entnum)) return true;
		--remaining;
		const auto &entity = entities.entities[link->entnum];
		if (link->entnum != ignore && entity.i.valid && entity.i.solid) return true;
	}
	return false;
}

static int EntityArea(int number) {
	const auto &entity = AAS_EntityWorld().entities[number];
	if (entity.areaWorld != aas_currentWorld) return 0;
	int first = 0, remaining = aasworld.linkheapsize;
	for (auto *link = entity.areas; link; link = link->next_area) {
		if (remaining <= 0 || !Link(link) || link->entnum != number || !Area(link->areanum)) return 0;
		--remaining;
		if (!first) first = link->areanum;
		const auto &settings = aasworld.areasettings[link->areanum];
		if ((settings.areaflags & AREA_GROUNDED) || AAS_AreaSwim(link->areanum)) return link->areanum;
	}
	return first;
}

static float Length(const float *vector) {
	volatile float x = vector[0] * vector[0], y = vector[1] * vector[1], z = vector[2] * vector[2];
	volatile float sum = x + y;
	sum = sum + z;
	return static_cast<float>(std::sqrt(static_cast<double>(sum)));
}

static float Normalize(float *vector) {
	const float length = Length(vector);
	if (length) {
		volatile float inverse = 1.0f / length;
		for (int axis = 0; axis < 3; ++axis) vector[axis] *= inverse;
	}
	return length;
}

static float DistanceToSegment(const float *enemy, const float *start, const float *end) {
	vec3_t direction, offset, projected;
	VectorSubtract(end, start, direction);
	Normalize(direction);
	VectorSubtract(enemy, start, offset);
	const float along = DotProduct(offset, direction);
	VectorMA(start, along, direction, projected);
	for (int axis = 0; axis < 3; ++axis) {
		if ((projected[axis] > start[axis] + 0.1 && projected[axis] > end[axis] + 0.1)
			|| (projected[axis] < start[axis] - 0.1 && projected[axis] < end[axis] - 0.1)) {
			VectorSubtract(enemy, end, offset);
			return Length(offset);
		}
	}
	VectorSubtract(enemy, projected, offset);
	return Length(offset);
}

class Query {
	aas_t *world_ = aas_currentWorld;
	uint64_t token_;
	int first_ = 0, last_ = 0;
public:
	aas_rtcw_tactical_node_t *nodes;
	aas_rtcw_cast_callbacks_t callbacks = AAS_RTCWCastCallbacks();
	Query() {
		if (!world_->rtcwTacticalNodes) {
			world_->rtcwTacticalNodes = botlib::ClearedAlloc<aas_rtcw_tactical_node_t>(world_->numareas);
			world_->rtcwTacticalCapacity = world_->numareas;
		}
		nodes = world_->rtcwTacticalNodes;
		std::memset(nodes, 0, sizeof(*nodes) * world_->rtcwTacticalCapacity);
		if (!++nextToken) ++nextToken;
		token_ = world_->rtcwTacticalToken = nextToken;
	}
	void Finish() {
		if (world_->rtcwTacticalToken == token_) world_->rtcwTacticalToken = 0;
		aas_currentWorld = world_;
	}
	bool Resume() {
		aas_currentWorld = world_;
		return world_->initialized && world_->rtcwTacticalToken == token_
			&& world_->rtcwTacticalNodes == nodes && world_->rtcwTacticalCapacity == world_->numareas;
	}
	void Push(int area, unsigned short cost, const float *entry) {
		auto &node = nodes[area];
		node.cost = cost;
		VectorCopy(entry, node.entry);
		if (node.queued) return;
		node.queued = true;
		node.next = 0;
		if (last_) nodes[last_].next = area; else first_ = area;
		last_ = area;
	}
	int Pop() {
		const int area = first_;
		if (area) {
			first_ = nodes[area].next;
			if (!first_) last_ = 0;
			nodes[area].queued = false;
		}
		return area;
	}
};
// Com_Error may longjmp past this object. All allocations remain map-owned,
// and no C++ destructor is required on that path; map shutdown clears tokens.
static_assert(std::is_trivially_destructible<Query>::value, "Tactical query must survive nonlocal engine errors");

static bool Links(int area, int &first, int &end) {
	const auto &settings = aasworld.areasettings[area];
	first = settings.firstreachablearea;
	if (first < 0 || first > aasworld.reachabilitysize || settings.numreachableareas < 0
		|| settings.numreachableareas > aasworld.reachabilitysize - first) return false;
	end = first + settings.numreachableareas;
	return true;
}

static bool Permitted(const aas_reachability_t &reach, int flags, int enemy) {
	return reach.areanum != enemy && reach.traveltype >= 0 && reach.traveltype < MAX_TRAVELTYPES
		&& !(aasworld.travelflagfortype[reach.traveltype] & ~flags)
		&& !(AAS_AreaContentsTravelFlags(reach.areanum) & ~flags)
		&& !(aasworld.areasettings[reach.areanum].areaflags &
			(AREA_DISABLED | (Com_GameIsRTCWSP() ? AREA_LADDER : 0)));
}

static int SearchHide(Query &query, int entity, float *origin, int area, int enemy, float *enemyOrigin, int enemyArea, int flags) {
	const bool sp = Com_GameIsRTCWSP() != 0;
	if (!query.Resume()) return 0;
	if (enemyArea) AAS_AreaTravelTimeToGoalArea(area, origin, enemyArea, flags);
	vec3_t direction;
	VectorSubtract(enemyOrigin, origin, direction);
	const float enemyDistance = Normalize(direction);
	const int startVisible = query.callbacks.visible(enemyOrigin, enemy, origin, entity, 0);
	if (!query.Resume() || (startVisible != 0 && startVisible != 1)) return 0;
	query.Push(area, 0, origin);
	int best = 0;
	unsigned short bestCost = 0;
	while (const int current = query.Pop()) {
		int first, end;
		if (!Links(current, first, end)) return 0;
		auto &from = query.nodes[current];
		for (int index = first; index < end; ++index) {
			auto &reach = aasworld.reachability[index];
			if (!Area(reach.areanum) || !Position(reach.start) || !Position(reach.end)) return 0;
			if (!Permitted(reach, flags, enemyArea)) continue;
			const int candidate = reach.areanum;
			auto &node = query.nodes[candidate];
			unsigned short cost = from.cost + AAS_AreaTravelTime(current, from.entry, reach.start) + reach.traveltime;
			if (sp) ++hideWork;
			if (node.visited && node.visited < cost) continue;
			node.visited = cost;
			if (best == candidate) { best = 0; bestCost = 0; }
			if (bestCost && cost >= bestCost) continue;
			const float distance = DistanceToSegment(enemyOrigin, from.entry, reach.end);
			vec3_t offset;
			VectorSubtract(reach.end, origin, offset);
			if (enemyDistance > 32 && ((distance < enemyDistance && distance < 256)
				|| DotProduct(offset, direction) > enemyDistance * 0.5f)) continue;
			VectorSubtract(enemyOrigin, from.entry, offset);
			const float previous = Length(offset);
			if (enemyDistance > 32 && distance < previous) {
				// Coordinates bound this below UINT32_MAX. Preserve ushort wrap
				// without an undefined float-to-ushort overflow conversion.
				volatile float penalty = (previous - distance) * 10.0f;
				volatile float adjusted = static_cast<float>(cost) + penalty;
				cost = static_cast<unsigned short>(static_cast<uint32_t>(adjusted));
				if (bestCost && cost >= bestCost) continue;
			}
			if (Occupied(candidate, entity)) cost = static_cast<unsigned short>(cost + 1000);
			if (enemyArea && !startVisible && AAS_AreaVisible(enemyArea, candidate)) continue;
			if (bestCost && cost >= bestCost) continue;
			if (aasworld.rtcwVisibility[candidate].data && !AAS_AreaVisible(enemyArea, candidate)) {
				if (!Position(aasworld.rtcwWaypoints[candidate])) return 0;
				if (!node.visibility) {
					const int visible = query.callbacks.visible(enemyOrigin, enemy,
						aasworld.rtcwWaypoints[candidate], entity, 0);
					if (!query.Resume() || (visible != 0 && visible != 1)) return 0;
					node.visibility = visible ? 1 : 2;
				}
				if (node.visibility == 2) { best = candidate; bestCost = cost; }
			}
			if (hideWork++ > (sp ? 3000 : 4000)) continue;
			query.Push(candidate, cost, reach.end);
		}
	}
	return best;
}

static int Hide(int entity, float *origin, int area, int enemy, float *enemyOrigin, int enemyArea, int flags) {
	if (entity < 0) { entity = -entity - 1; hideTime = 0; }
	if (hideTime == AAS_Time() && (!Com_GameIsRTCWSP() || hideWork >= 3000)) return 0;
	hideTime = AAS_Time();
	hideWork = 0;
	Query query;
	const int result = SearchHide(query, entity, origin, area, enemy, enemyOrigin, enemyArea, flags);
	query.Finish();
	return result;
}

static int SearchAttack(Query &query, int entity, int rangeEntity, int enemy, float range, int flags) {
	if (!query.Resume()) return 0;
	vec3_t origin, rangeOrigin, enemyOrigin;
	const auto &entities = AAS_EntityWorld();
	VectorCopy(entities.entities[entity].i.origin, origin);
	VectorCopy(entities.entities[rangeEntity].i.origin, rangeOrigin);
	VectorCopy(entities.entities[enemy].i.origin, enemyOrigin);
	if (!Position(origin) || !Position(rangeOrigin) || !Position(enemyOrigin)) return 0;
	const bool sp = Com_GameIsRTCWSP() != 0;
	const int area = sp ? BotFuzzyPointReachabilityArea(origin) : EntityArea(entity);
	const int rangeArea = sp ? BotFuzzyPointReachabilityArea(rangeOrigin) : EntityArea(rangeEntity);
	const int enemyArea = sp ? BotFuzzyPointReachabilityArea(enemyOrigin) : EntityArea(enemy);
	if (!Area(area) || !Area(rangeArea) || enemyArea < 0 || enemyArea >= aasworld.numareas) return 0;
	AAS_AreaTravelTimeToGoalArea(area, origin, enemyArea, flags);
	query.Push(rangeArea, 0, rangeOrigin);
	int best = 0, work = 0;
	unsigned short bestCost = 0;
	while (const int current = query.Pop()) {
		int first, end;
		if (!Links(current, first, end)) return 0;
		auto &from = query.nodes[current];
		for (int index = first; index < end; ++index) {
			auto &reach = aasworld.reachability[index];
			if (!Area(reach.areanum) || !Position(reach.start) || !Position(reach.end)) return 0;
			if (!Permitted(reach, flags, enemyArea)) continue;
			const int candidate = reach.areanum;
			auto &node = query.nodes[candidate];
			if (node.visited) continue;
			if (work++ > 200) return best;
			const unsigned short cost = from.cost + AAS_AreaTravelTime(current, from.entry, reach.start) + reach.traveltime;
			if (!Position(aasworld.rtcwWaypoints[candidate])) return 0;
			vec3_t offset;
			VectorSubtract(rangeOrigin, aasworld.rtcwWaypoints[candidate], offset);
			if (Length(offset) > range) continue;
			const unsigned short sourceCost = AAS_AreaTravelTimeToGoalArea(area, origin, candidate, flags);
			if (bestCost && sourceCost >= bestCost) continue;
			if (aasworld.rtcwVisibility[candidate].data && AAS_AreaVisible(enemyArea, candidate)) {
				if (!node.visibility) {
					work += 10;
					const int possible = query.callbacks.attack(entity, enemy, aasworld.rtcwWaypoints[candidate], 0, 0);
					if (!query.Resume() || (possible != 0 && possible != 1)) return 0;
					node.visibility = possible ? 2 : 1;
				}
				if (node.visibility == 2) { best = candidate; bestCost = sourceCost; }
			}
			node.visited = cost;
			query.Push(candidate, cost, reach.end);
		}
	}
	return best;
}

static int Attack(int entity, int rangeEntity, int enemy, float range, int flags) {
	if (attackTime == AAS_Time()) return 0;
	attackTime = AAS_Time();
	Query query;
	const int result = SearchAttack(query, entity, rangeEntity, enemy, range, flags);
	query.Finish();
	return result;
}

} // namespace fntech3::rtcw::tactical

bool AAS_RTCWHidePosition(const float *source, int sourceEntity, int sourceArea,
	const float *enemy, int enemyEntity, int enemyArea, float *position) {
	using namespace fntech3::rtcw::tactical;
	if (!Ready() || !position || !Position(source) || !Position(enemy) || !Area(sourceArea)
		|| enemyArea < 0 || enemyArea >= aasworld.numareas || !Entity(enemyEntity)) return false;
	const int64_t sourceNumber = sourceEntity < 0 ? -static_cast<int64_t>(sourceEntity) - 1 : sourceEntity;
	if ((!Com_GameIsRTCWSP() && sourceEntity < 0) || sourceNumber > INT_MAX || !Entity(static_cast<int>(sourceNumber))) return false;
	const auto callbacks = AAS_RTCWCastCallbacks();
	if (!callbacks.visible || !callbacks.available || !callbacks.available()) return false;
	vec3_t origin, target;
	VectorCopy(source, origin);
	VectorCopy(enemy, target);
	// Original SP's macro keeps jump pads enabled; MP masks them out. Both
	// permit slime, which Quake's default mask omits (see the contract probe).
	const int flags = Com_GameIsRTCWSP() ? 20713406 : 20451262;
	const int found = Hide(sourceEntity, origin, sourceArea, enemyEntity, target, enemyArea, flags);
	if (!found) return false;
	VectorCopy(aasworld.rtcwWaypoints[found], position);
	return true;
}

int AAS_RTCWAttackPosition(int sourceEntity, int rangeEntity, int enemyEntity,
	float range, int travelFlags, float *position) {
	using namespace fntech3::rtcw::tactical;
	if (!Ready() || !position || !Entity(sourceEntity) || !Entity(rangeEntity) || !Entity(enemyEntity)
		|| !fntech3::rtcw::routing::Finite(range) || range < 0) return 0;
	const auto callbacks = AAS_RTCWCastCallbacks();
	if (!callbacks.attack || !callbacks.available || !callbacks.available()) return 0;
	const int found = Attack(sourceEntity, rangeEntity, enemyEntity, range, travelFlags);
	if (found) VectorCopy(aasworld.rtcwWaypoints[found], position);
	return found;
}
#endif
