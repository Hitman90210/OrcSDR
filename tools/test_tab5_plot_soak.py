"""Bounded COM39 HOME/FM soak; logs no pairing material. Run from repository root."""
import argparse,json,re,sys,time
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'tools'))
from help_media import Tab5
p=argparse.ArgumentParser();p.add_argument('--port',default='COM39');p.add_argument('--seconds',type=int,default=1800);p.add_argument('--output',type=Path,required=True);p.add_argument('--pairing-key',type=Path,required=True);a=p.parse_args()
if a.seconds < 60:p.error('--seconds must be at least 60')
a.output.mkdir(parents=True,exist_ok=True)
log=a.output/'events.jsonl'
def emit(kind,**data):
 item={'kind':kind,'time':time.time(),**data}
 with log.open('a',encoding='utf-8') as f:f.write(json.dumps(item)+'\n')
 print(json.dumps(item),flush=True)
c=Tab5(a.port,a.pairing_key)
faults=[]
def inspect(line):
 if any(x in line for x in ['Guru Meditation','abort()','Task watchdog','Brownout','ESP-ROM:','RTL_MAIN_STALL']):
  faults.append(line);emit('fault',line=line)
def query(cmd,prefix,auth=False):
 if auth or cmd.startswith('RTL_SOUND '):c.authenticate()
 c.send(cmd);deadline=time.monotonic()+25
 while time.monotonic()<deadline:
  line=c.serial.readline().decode('utf-8','replace').strip()
  inspect(line)
  if line.startswith(prefix):
   emit('reply',command=cmd,reply=line)
   if any(x in line for x in ['ERROR','INVALID','UNAVAILABLE']):raise RuntimeError(line)
   return line
 raise TimeoutError(cmd)
def numbers(line):return {k:int(v) for k,v in re.findall(r'(\w+)=(\d+)',line)}
def home_progress():
 assert 'active=0' in query('RTL_LAB STATUS','RTL_LAB_STATUS')
 assert 'screen=home' in query('RTL_UI STATUS','RTL_UI_STATUS')
 first=numbers(query('HOME_PLOT STATUS','HOME_PLOT'));time.sleep(2)
 last=numbers(query('HOME_PLOT STATUS','HOME_PLOT'))
 assert last['frames']>first['frames'],'HOME plot did not advance'
 return last
initial_freq=query('RTL_FREQ','RTL_FREQ_STATUS');m=re.search(r'band=(\S+) frequency_hz=(\d+)',initial_freq);initial_band,initial_hz=m.groups()
initial_sound=numbers(query('RTL_SOUND','RTL_SOUND_STATUS'))['enabled']
summary={'pass':False,'port':a.port}
try:
 query('RTL_TUNE FM 96100000','RTL_TUNE_',True);time.sleep(8)
 query('RTL_UI OPEN HOME','RTL_UI_OPEN_OK',True)
 query('UI_SELF_CHECK','UI_SELF_CHECK')
 assert 'pass=1' in query('RTL_UI_REGRESSION CHECK','RTL_UI_REGRESSION_RESULT')
 for cycle in range(20):
  query('RTL_UI OPEN RF_LAB','RTL_UI_OPEN_OK',True);time.sleep(.5)
  assert 'active=1' in query('RTL_LAB STATUS','RTL_LAB_STATUS')
  close=['RTL_UI OPEN HOME','RTL_LAB CLOSE','RTL_UI TAP 1069 37','RTL_KEYBOARD KEY ESC'][cycle%4]
  prefix=['RTL_UI_OPEN_OK','RTL_LAB_OK','RTL_UI_TAP_OK','RTL_KEYBOARD_KEY_OK'][cycle%4]
  query(close,prefix,True);time.sleep(1);home_progress()
  emit('navigation',cycle=cycle+1,path=close,passed=True)
 query('RTL_UI OPEN SETTINGS','RTL_UI_OPEN_OK',True);time.sleep(1)
 query('RTL_UI OPEN HOME','RTL_UI_OPEN_OK',True);home_progress()
 assert 'pass=1' in query('RTL_UI_REGRESSION RUN','RTL_UI_REGRESSION_RESULT',True)
 query('RTL_TUNE FM 96100000','RTL_TUNE_',True);time.sleep(5)
 query('RTL_UI OPEN HOME','RTL_UI_OPEN_OK',True)
 query('RTL_SOUND ON','RTL_SOUND_OK');time.sleep(5)
 faults.clear()
 baseline=json.loads(query('RTL_AUDIO_TEST STATUS','{"type":"rtl_audio_test"'))
 first_plot=numbers(query('HOME_PLOT STATUS','HOME_PLOT'))
 previous=baseline;previous_plot=first_plot;started=time.monotonic();next_sample=started+60
 samples=[]
 while time.monotonic()-started<a.seconds:
  line=c.serial.readline().decode('utf-8','replace').strip()
  if line:inspect(line)
  if time.monotonic()>=next_sample or time.monotonic()-started>=a.seconds:
   current=json.loads(query('RTL_AUDIO_TEST STATUS','{"type":"rtl_audio_test"'))
   plot=numbers(query('HOME_PLOT STATUS','HOME_PLOT'))
   dt=plot['elapsed_ms']-previous_plot['elapsed_ms']
   assert dt>0 and plot['frames']>previous_plot['frames'],'plot reset/stall'
   deltas={k:current[k]-baseline[k] for k in ['audio_chunks','audio_drops','ring_overruns','submit_failures','usb_overruns','usb_drops']}
   sample={'seconds':round(time.monotonic()-started,1),'updates_s':round((plot['frames']-previous_plot['frames'])*1000/dt,2),'p50_ms':plot['p50_ms'],'p95_ms':plot['p95_ms'],'free_heap':current['free_heap'],'effective_sps':current['effective_sps'],'deltas':deltas}
   samples.append(sample);emit('sample',**sample)
   query('RTL_HEALTH','RTL_HEALTH_STATUS')
   query('RTL_WIFI_COEX_STATUS','RTL_WIFI_COEX_STATUS')
   assert not faults,'runtime fault marker'
   assert current['speaker_running']==1,'speaker stopped despite sound being enabled'
   assert all(deltas[k]==0 for k in ['audio_drops','ring_overruns','submit_failures','usb_overruns','usb_drops']), 'drop counter increased'
   assert current['audio_chunks']>previous['audio_chunks'],'audio did not advance'
   if len(samples)>1:
    previous_delta=samples[-2]['deltas']['audio_chunks']-(samples[-3]['deltas']['audio_chunks'] if len(samples)>2 else 0)
    chunk_delta=current['audio_chunks']-previous['audio_chunks']
    assert chunk_delta >= previous_delta * .8, 'audio production fell by more than 20 percent; inspect state/workload'
   previous=current;previous_plot=plot;next_sample+=60
 summary.update(passed_seconds=round(time.monotonic()-started,1),samples=samples,navigation_cycles=20)
 summary['pass']=True
except Exception as e:
 summary['error']=str(e);emit('failure',error=str(e));raise
finally:
 try:
  query(f'RTL_TUNE {initial_band} {initial_hz}','RTL_TUNE_',True);time.sleep(2)
  query('RTL_UI OPEN HOME','RTL_UI_OPEN_OK',True)
  query('RTL_SOUND '+('ON' if initial_sound else 'OFF'),'RTL_SOUND_OK')
  summary['restored']=query('RTL_UI STATUS','RTL_UI_STATUS')
 finally:
  summary['faults']=faults;(a.output/'summary.json').write_text(json.dumps(summary,indent=2));emit('summary',**summary);c.close()
