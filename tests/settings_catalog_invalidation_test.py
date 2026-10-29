"""Exercise the production Settings catalog observation and removal cursor."""
from pathlib import Path
import re
import shutil
import subprocess

root = Path(__file__).resolve().parents[1]
source = (root / "apps/orcsdr-tab5/ui/settings_app.cpp").read_text(encoding="utf-8")
start = source.index("\n", source.index("  const bool rtc_flipped =")) + 1
end = source.index("  const bool location_changed =", start)
condition = source[start:end]
observe = re.search(
    r"  if \([^\n]+\) g_catalog_cursor\.catalog_changed\(state_value\.catalog_packs\);",
    source,
).group()
harness = r'''
#include "../../../apps/orcsdr-tab5/ui/map_catalog_paging.hpp"
#include <cassert>
#include <cstring>
#include <cstdio>
enum class Section { data_maps, system };
Section g_section=Section::data_maps;
struct Pack {char id[20]{};bool installed=false;};
struct State {
 bool catalog_ready=false,catalog_busy=false;
 unsigned catalog_progress_percent=0;
 char catalog_message[40]{},catalog_date[20]{};
 Pack catalog_packs[2]{};
} g_state;
orcsdr::map_catalog_paging::Cursor g_catalog_cursor;
bool update_catalog(const State& state_value) {
'''
harness += condition + observe + "\n g_state=state_value;return catalog_changed;\n}\n"
harness += r'''
int main() {
 std::strcpy(g_state.catalog_packs[0].id,"original");
 g_state.catalog_packs[0].installed=true;
 assert(g_catalog_cursor.remove(g_state.catalog_packs,0,false)==-1);
 assert(g_catalog_cursor.armed==0);
 g_section=Section::system;
 State changed=g_state;
 std::strcpy(changed.catalog_packs[0].id,"different_pack");
 assert(!update_catalog(changed)); // Hidden catalog changes must not repaint System.
 assert(g_catalog_cursor.armed==-1); // The old confirmation must not target the replacement.
 g_section=Section::data_maps;
 assert(g_catalog_cursor.remove(g_state.catalog_packs,0,false)==-1);
 assert(g_catalog_cursor.armed==0);
 assert(!update_catalog(g_state)); // An unchanged snapshot must preserve the new confirmation.
 assert(g_catalog_cursor.armed==0);
 assert(g_catalog_cursor.remove(g_state.catalog_packs,0,false)==0);
 changed=g_state;++changed.catalog_progress_percent;
 assert(update_catalog(changed)); // Visible catalog updates still request a repaint.
 std::puts("PASS: hidden catalog invalidates removal; unchanged snapshot preserves confirmation; repaint stays scoped");
}
'''
folder = root / ".local/maps/settings-catalog-invalidation"
folder.mkdir(parents=True, exist_ok=True)
cpp = folder / "test.cpp"
cpp.write_text(harness, encoding="utf-8")
exe = folder / "test"


def linux(path):
    return "/mnt/" + str(path)[0].lower() + str(path)[2:].replace("\\", "/")


if shutil.which("g++"):
    compile_command = ["g++", "-std=c++17", "-Wall", "-Wextra", "-Werror", str(cpp), "-o", str(exe)]
    run_command = [str(exe)]
else:
    compile_command = ["wsl", "g++", "-std=c++17", "-Wall", "-Wextra", "-Werror", linux(cpp), "-o", linux(exe)]
    run_command = ["wsl", linux(exe)]
subprocess.run(compile_command, check=True)
subprocess.run(run_command, check=True)
