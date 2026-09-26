"""Official Elite Force BSP inventory observed in the retail installation.

pak0.pk3 ships the base maps and pak1/pak3 add the Expansion Pack's; pak2 has
no BSP. The 33 Holomatch maps are the ones scripts/arenas.txt and
scripts/xpack.arena declare, which leaves the 66 single-player and tour maps.
Ten single-player maps start with an underscore and ten more live in the
maps/tour/ subdirectory, so entries are map paths rather than bare names.
Observed 2026-09-22; source identities are recorded under audioZones in
docs/fntech3/2026-09-22-stvef-presentation-assets.json.
"""

STOCK_STVEF_MAPS = {
    "stvef-sp": (
        "_brig", "_holodeck_camelot", "_holodeck_firingrange", "_holodeck_garden",
        "_holodeck_highnoon", "_holodeck_minigame", "_holodeck_proton", "_holodeck_proton2",
        "_holodeck_temple", "_holodeck_warlord", "borg1", "borg2", "borg3", "borg4", "borg5",
        "borg6", "dn1", "dn2", "dn3", "dn4", "dn5", "dn6", "dn8", "forge1", "forge2", "forge3",
        "forge4", "forge5", "forgeboss", "holodeck", "scav1", "scav2", "scav3", "scav3b",
        "scav4", "scav5", "scavboss", "stasis1", "stasis2", "stasis3", "tour/deck01",
        "tour/deck02", "tour/deck03", "tour/deck04", "tour/deck05", "tour/deck08",
        "tour/deck09", "tour/deck10", "tour/deck11", "tour/deck15", "tutorial", "voy1",
        "voy13", "voy14", "voy15", "voy16", "voy17", "voy2", "voy20", "voy3", "voy4", "voy5",
        "voy6", "voy7", "voy8", "voy9",
    ),
    "stvef-mp": (
        "ctf_and1", "ctf_breach", "ctf_dn1", "ctf_for1", "ctf_kln1", "ctf_kln2", "ctf_neptune",
        "ctf_oldwest", "ctf_reservoir", "ctf_singularity", "ctf_spyglass2", "ctf_stasis",
        "ctf_voy1", "ctf_voy2", "hm_altar", "hm_blastradius", "hm_borg1", "hm_borg2",
        "hm_borg3", "hm_borgattack", "hm_cam", "hm_dn1", "hm_dn2", "hm_for1", "hm_for2",
        "hm_kln1", "hm_noon", "hm_raven", "hm_scav1", "hm_temple", "hm_voy1", "hm_voy2",
        "hm_voy3",
    ),
}


if __name__ == "__main__":
    for profile, names in STOCK_STVEF_MAPS.items():
        for name in names:
            print(f"{profile}/baseEF/maps/{name}.azb")
