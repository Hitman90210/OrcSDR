"""Exercise production SD open modes against real files, including retry writes."""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
text = (root / 'apps/orcsdr-tab5/ui/orcsdr_storage.cpp').read_text()
start = text.index('  const bool appending =', text.index('File FileSystem::open('))
end = text.index('  if (state->stream && writing)', start)
production = text[start:end]
harness = r'''
#include <cassert>
#include <cstdio>
#include <cstring>
#include <string>
struct State { FILE* stream=nullptr; };
FILE* open_file(const char* path,const char* mode) {
  std::string mounted(path); State value; State* state=&value;
  /* PRODUCTION */
  (void)writing;
  return state->stream;
}
int main(int argc,char** argv) {
  assert(argc==2);
  FILE* f=open_file(argv[1],"w+"); assert(f);
  assert(fwrite("header\npartial",1,14,f)==14); assert(fclose(f)==0);
  f=open_file(argv[1],"r+"); assert(f);
  assert(fseek(f,7,SEEK_SET)==0);
  assert(fwrite("complete\n",1,9,f)==9); assert(fclose(f)==0);
  f=open_file(argv[1],"a"); assert(f);
  assert(fwrite("tail\n",1,5,f)==5); assert(fclose(f)==0);
  f=open_file(argv[1],"r"); assert(f); char data[64]={};
  assert(fread(data,1,63,f)==21); assert(fclose(f)==0);
  assert(strcmp(data,"header\ncomplete\ntail\n")==0);
  puts("STORAGE_UPDATE_MODE_PASS");
}
'''.replace('/* PRODUCTION */', production)
with tempfile.TemporaryDirectory() as directory:
    work = Path(directory)
    (work / 'test.cpp').write_text(harness)
    subprocess.run(['g++', '-std=c++17', '-Wall', '-Wextra', '-Werror',
                    str(work / 'test.cpp'), '-o', str(work / 'test')], check=True)
    subprocess.run([str(work / 'test'), str(work / 'log.csv')], check=True)
