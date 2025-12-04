import subprocess
from rabbitvcs.util import helper

def call_tgit(command, args=None, block=False):
    args = args or []
    proc = subprocess.Popen(['tgit', command, *args])
    if block: proc.wait()
    return proc

original_launch_ui = helper.launch_ui_window
def launch_ui_window(filename, args=[], block=False):
    #with open('/tmp/rabbit.txt', 'a') as f: f.write(f"launch_ui_window(filename='{filename}', args={args}, block={block})\n")

    #Handle a base vs working diff request
    if filename == 'diff' and len(args) >= 2:
        path1, path2 = args[-2], args[-1]
        base_path = path1.rsplit('@', 1)[0] if '@base' in path1 else None
        work_path = path2.rsplit('@', 1)[0] if '@working' in path2 else None
        if base_path == work_path and base_path:
            return call_tgit(filename, [base_path], block)

    #Handle pass through command requests
    if filename in ['commit', 'log', 'push'] and len(args) >= 1:
        return call_tgit(filename, [args[-1]], block)
    if (command_name := {'update':'fetch'}.get(filename)) is not None:
        return call_tgit(command_name, [args[-1]], block)

    #with open('/tmp/rabbit.txt', 'a') as f: f.write(f"CALLING ORIGINAL\n")
    return original_launch_ui(filename, args, block)
helper.launch_ui_window = launch_ui_window