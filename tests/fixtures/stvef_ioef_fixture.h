/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later.
 * Synthetic input shared by the optional reference generator and engine tests.
 * The includer defines IOEF_Entity, IOEF_Player and IOEF_Command. */
static void IOEF_FillEntity(IOEF_Entity *out) {
	IOEF_Entity entity;
	memset(&entity, 0, sizeof(entity));
	int i;
	entity.number = 7;
	entity.eType = 3; entity.eFlags = 0xabcdef;
	entity.pos.trType = TR_LINEAR; entity.pos.trTime = 12345; entity.pos.trDuration = 678;
	entity.apos.trType = TR_SINE; entity.apos.trTime = 23456; entity.apos.trDuration = 789;
	for (i = 0; i < 3; ++i) {
		entity.pos.trBase[i] = 1.0f + i; entity.pos.trDelta[i] = -2.5f - i;
		entity.apos.trBase[i] = 0.25f + i; entity.apos.trDelta[i] = -4096.0f + i;
		entity.origin[i] = 4095.0f - i; entity.origin2[i] = 4096.0f + i;
		entity.angles[i] = -3.0f - i; entity.angles2[i] = 3.25f + i;
	}
	entity.time = 34567; entity.time2 = -45678;
	entity.otherEntityNum = 1001; entity.otherEntityNum2 = 1002; entity.groundEntityNum = 1023;
	entity.loopSound = 0xabcd; entity.constantLight = 0x12345678;
	entity.modelindex = 201; entity.modelindex2 = 202; entity.frame = 50000; entity.clientNum = 63;
	entity.solid = 0xfedcba; entity.event = 999; entity.eventParm = 234;
	entity.powerups = 0xcdef; entity.weapon = 15; entity.legsAnim = 128; entity.torsoAnim = 129;
	*out = entity;
}
static void IOEF_FillPlayer(IOEF_Player *out) {
	IOEF_Player player;
	memset(&player, 0, sizeof(player));
	int i;
	player.commandTime = 123456; player.pm_type = 3; player.bobCycle = 201; player.pm_flags = 0xbeef;
	player.pm_time = -100; player.weaponTime = -200; player.gravity = 800; player.speed = 320;
	for (i = 0; i < 3; ++i) {
		player.origin[i] = 12.0f + i; player.velocity[i] = -0.5f - i;
		player.delta_angles[i] = 30000 + i; player.viewangles[i] = 4096.5f + i;
	}
	player.groundEntityNum = 1023; player.legsTimer = 250; player.torsoTimer = 4095;
	player.legsAnim = 128; player.torsoAnim = 129; player.movementDir = 7; player.eFlags = 0xfffe;
	player.eventSequence = 4321;
	for (i = 0; i < 4; ++i) { player.events[i] = 200 + i; player.eventParms[i] = 230 + i; }
	player.externalEvent = 777; player.externalEventParm = 255; player.clientNum = 63;
	player.weapon = 15; player.weaponstate = 3; player.viewheight = -8;
	player.damageEvent = 16; player.damageYaw = 17; player.damagePitch = 18; player.damageCount = 19;
	player.damageShieldCount = 20; player.introTime = -76543;
	for (i = 0; i < 16; ++i) {
		player.stats[i] = -10 - i; player.persistant[i] = 30 + i;
		player.ammo[i] = 40 + i; player.powerups[i] = 100000 + i;
	}
	*out = player;
}
static void IOEF_FillCommand(IOEF_Command *out) {
	IOEF_Command command;
	memset(&command, 0, sizeof(command));
	command.serverTime = 100017;
	command.angles[0] = 65000; command.angles[1] = 54321; command.angles[2] = 43210;
	command.forwardmove = -128; command.rightmove = 127; command.upmove = -1;
	command.buttons = 0xab; command.weapon = 9;
	*out = command;
}
