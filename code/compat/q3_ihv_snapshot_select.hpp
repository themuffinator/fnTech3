/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#ifndef FNTECH3_Q3_IHV_SNAPSHOT_SELECT_HPP
#define FNTECH3_Q3_IHV_SNAPSHOT_SELECT_HPP

#include "q3_ihv_game_abi.h"
#include "q3_ihv_native_boundary.hpp"
#include <algorithm>
#include <cstddef>
#include <cstring>

namespace fntech3::q3ihv {

/* The visibility walk SV_AddEntitiesVisibleFromPoint 0x41b040 performs for one
 * view point. Its collision and link queries come from the host so the same
 * walk serves the engine and the optional original-function comparison. IHV
 * keeps Quake II's record: an entity whose cluster list overflowed is tested
 * from the node its bounds straddled. */
enum { FNTECH3_Q3_IHV_MAX_ENT_CLUSTERS = 16 };

struct SnapshotLink {
	int numClusters = 0;
	int clusternums[FNTECH3_Q3_IHV_MAX_ENT_CLUSTERS]{};
	int headnode = 0;
	int areanum = -1;
	int areanum2 = -1;
};

struct SnapshotHost {
	void *context = nullptr;
	int pvsBytes = 0;
	int entityCount = 0;
	int (*PointLeafnum)(void *, const float *) = nullptr;
	int (*LeafArea)(void *, int) = nullptr;
	int (*LeafCluster)(void *, int) = nullptr;
	int (*WriteAreaBits)(void *, unsigned char *, int) = nullptr;
	const unsigned char *(*ClusterPVS)(void *, int) = nullptr;
	bool (*AreasConnected)(void *, int, int) = nullptr;
	bool (*HeadnodeVisible)(void *, int, const unsigned char *) = nullptr;
	fntech3_q3_ihv_entity_t *(*EntityAt)(void *, int) = nullptr;
	bool (*ReadLink)(void *, int, SnapshotLink &) = nullptr;
	/* The per-snapshot mark that keeps a portal from adding an entity twice. */
	bool (*Added)(void *, int) = nullptr;
	void (*MarkAdded)(void *, int) = nullptr;
	/* The original prints when an entity's own number disagrees with its slot
	 * and then rewrites that number in place. */
	void (*RepairedNumber)(void *, int, int) = nullptr;
	bool Complete() const {
		return PointLeafnum && LeafArea && LeafCluster && WriteAreaBits && ClusterPVS
			&& AreasConnected && HeadnodeVisible && EntityAt && ReadLink && Added
			&& MarkAdded && RepairedNumber && pvsBytes > 0 && entityCount > 0;
	}
};

class SnapshotSelection {
public:
	int Count() const { return count_; }
	const int *Numbers() const { return numbers_; }
	int Number(int index) const { return index >= 0 && index < count_ ? numbers_[index] : -1; }
	/* The original sorts the selected numbers before copying their states. */
	void Sort() { std::sort(numbers_, numbers_ + count_); }
	bool FromPoint(const SnapshotHost &host, const float *origin,
		unsigned char (&areabits)[FNTECH3_Q3_IHV_AREABITS_BYTES], int &areabytes) {
		if (!host.Complete() || !origin) return false;
		const int leaf = host.PointLeafnum(host.context, origin);
		const int clientarea = host.LeafArea(host.context, leaf);
		areabytes = host.WriteAreaBits(host.context, areabits, clientarea);
		if (areabytes < 0 || areabytes > FNTECH3_Q3_IHV_AREABITS_BYTES) return false;
		const unsigned char *pvs = host.ClusterPVS(host.context, host.LeafCluster(host.context, leaf));
		if (!pvs) return false;
		for (int number = 1; number < host.entityCount; ++number) {
			fntech3_q3_ihv_entity_t *entity = host.EntityAt(host.context, number);
			if (!entity) return false;
			if (!entity->inuse) continue;
			const int published = EntityNumber(*entity);
			if (published != number) {
				host.RepairedNumber(host.context, number, published);
				if (!SetEntityNumber(*entity, number)) return false;
			}
			if (!entity->linked || (entity->svFlags & FNTECH3_Q3_IHV_SVF_NOCLIENT)) continue;
			if (host.Added(host.context, number)) continue;
			if (entity->svFlags & FNTECH3_Q3_IHV_SVF_BROADCAST) { Add(host, number, *entity); continue; }
			SnapshotLink link;
			if (!host.ReadLink(host.context, number, link)) return false;
			if (link.numClusters < -1 || link.numClusters > FNTECH3_Q3_IHV_MAX_ENT_CLUSTERS) return false;
			if (!host.AreasConnected(host.context, clientarea, link.areanum)
				&& !host.AreasConnected(host.context, clientarea, link.areanum2)) continue;
			if (!Visible(host, link, pvs)) continue;
			Add(host, number, *entity);
			if (entity->svFlags & FNTECH3_Q3_IHV_SVF_PORTAL) {
				float origin2[3];
				std::memcpy(origin2, entity->state + FNTECH3_Q3_IHV_ENTITY_ORIGIN2, sizeof origin2);
				if (!FromPoint(host, origin2, areabits, areabytes)) return false;
			}
		}
		return true;
	}

private:
	bool Visible(const SnapshotHost &host, const SnapshotLink &link, const unsigned char *pvs) const {
		if (link.numClusters == -1) return host.HeadnodeVisible(host.context, link.headnode, pvs);
		for (int i = 0; i < link.numClusters; ++i) {
			const int cluster = link.clusternums[i];
			// A cluster outside the supplied vector cannot be read for a bit.
			if (cluster < 0 || cluster >= host.pvsBytes * 8) continue;
			if (pvs[cluster >> 3] & (1 << (cluster & 7))) return true;
		}
		return false;
	}
	void Add(const SnapshotHost &host, int number, const fntech3_q3_ihv_entity_t &entity) {
		if (host.Added(host.context, number)) return;
		host.MarkAdded(host.context, number);
		if (count_ >= FNTECH3_Q3_IHV_SNAPSHOT_ENTITIES) return;
		numbers_[count_++] = EntityNumber(entity);
	}
	int count_ = 0;
	int numbers_[FNTECH3_Q3_IHV_SNAPSHOT_ENTITIES]{};
};

} // namespace fntech3::q3ihv
#endif
