"""Legacy import shim for scripts that have not yet adopted fnTech3 naming."""

if __package__:
    from .fntech3_meta import *
    from .fntech3_meta import __all__
else:
    from fntech3_meta import *
    from fntech3_meta import __all__
