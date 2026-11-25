from pathlib import Path
import traceback
try:
    from PCVRLauncher import main
    main()
except Exception:
    (Path(__file__).resolve().parent/'PCVR-Startup-Error.log').write_text(traceback.format_exc())
    import ctypes
    ctypes.windll.user32.MessageBoxW(0,'Portal PCVR launcher could not start. See PCVR-Startup-Error.log. Python3 with Tk is required.','Portal PCVR startup',0)
