"""Strict connectionless identity evidence shared by runtime probe harnesses."""
import re


def server_info(payload, response_name, mapname, challenge, protocol=None):
    """Accept only complete, unambiguous identity for this challenged request."""
    prefix = b'\xff\xff\xff\xff' + response_name.encode('ascii') + b'\n'
    if not payload.startswith(prefix):
        return None
    body = payload[len(prefix):].rstrip(b'\x00')
    # SVC_Status separates the player list even when that list is empty.
    if response_name == 'statusResponse' and b'\n' not in body:
        return None
    info = body.split(b'\n', 1)[0].rstrip(b'\r').decode('latin1')
    if not info.startswith('\\') or '\x00' in info:
        return None
    fields = info.split('\\')[1:]
    if len(fields) % 2:
        return None
    names = fields[::2]
    # Engine Info_ValueForKey compares keys without regard to case. A dict
    # must not hide a second spelling or choose the last duplicate value.
    if any(not name for name in names) or len({name.casefold() for name in names}) != len(names):
        return None
    result = dict(zip(names, fields[1::2]))
    if (result.get('mapname') != mapname or result.get('challenge') != challenge
            or (protocol is not None and result.get('protocol') != str(protocol))):
        return None
    return result


def observed_port(log):
    """Read the child's bound port after NET_OpenIP's collision fallback."""
    found = re.findall(r'^"net_port" is:"([0-9]+)(?:\^7)?"', log, re.M)
    return int(found[-1]) if found else None
