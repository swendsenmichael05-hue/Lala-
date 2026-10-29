"""Exercise the exact published revision with touch input; no mocked page responses."""
import json, os, pathlib, time
import requests
from playwright.sync_api import sync_playwright

REF = os.environ.get('GITHUB_SHA', 'veil-vow-browser')
RAW = 'https://raw.githubusercontent.com/swendsenmichael05-hue/Lala-/' + REF + '/index.html'
PLAY = 'https://htmlpreview.github.io/?' + RAW
OUT = pathlib.Path('test-output'); OUT.mkdir(exist_ok=True)
report = {'version':'4.0','play_url':PLAY,'physical_device':False,'browsers':[]}
for attempt in range(8):
    r=requests.get(RAW,timeout=30)
    if r.status_code==200 and r.text==pathlib.Path('index.html').read_text(): break
    time.sleep(4)
else: raise RuntimeError('The public source differs from this revision')

def state(p): return p.evaluate('window.VV.get()')
def phase(p,name):
    p.wait_for_function('(x)=>window.VV&&window.VV.get()&&window.VV.get().phase===x',arg=name,timeout=15000)
    assert p.locator('#error-text').count()==0, 'Game recovery screen appeared'
def tap(p,action): p.locator('[data-do="'+action+'"]').tap(timeout=15000)
def open_game(p):
    p.goto(PLAY,wait_until='domcontentloaded',timeout=90000)
    p.wait_for_function("window.VV&&window.VV.version==='4.0'&&document.documentElement.dataset.gameReady==='true'",timeout=60000)
    assert p.locator('#build').inner_text()=='pocket edition · 4.0'
def start(p):
    p.locator('#start-form button').tap(); phase(p,'choose')
def choices(p,mode='help',index=0):
    tap(p,'choose-'+str(index));phase(p,'help');tap(p,'support-'+mode);phase(p,'ready')
def fixed_roll(p,main=6,helper=6):
    p.evaluate('''([main,helper])=>{window.__originalRandom=crypto.getRandomValues;let i=0;Object.defineProperty(crypto,'getRandomValues',{configurable:true,value:arr=>{arr[0]=[helper-1,main-1][i++]??5;return arr}})}''',[main,helper])
    tap(p,'roll');phase(p,'result')
    p.evaluate("Object.defineProperty(crypto,'getRandomValues',{configurable:true,value:window.__originalRandom})")
def new_trip(p):
    p.locator('#menu').tap();p.locator('[data-global="restart"]').tap();tap(p,'fresh');phase(p,'choose')

with sync_playwright() as w:
  for engine in ('chromium','webkit'):
    tests=[];errors=[]
    b=getattr(w,engine).launch()
    c=b.new_context(viewport={'width':390,'height':844},is_mobile=True,has_touch=True,device_scale_factor=2)
    p=c.new_page();p.on('pageerror',lambda e:errors.append(str(e)))
    try:
      open_game(p)
      p.locator('#one').fill('Moss');p.locator('#two').fill('Wren')
      start(p);first=state(p)
      assert first['players']==[{'name':'Moss'},{'name':'Wren'}]
      assert 'Moss, pick a plan.' in p.locator('#focus').inner_text()
      p.screenshot(path=str(OUT/(engine+'-encounter.png')),full_page=True)
      choices(p)
      assert p.locator('canvas').count()==2
      before=p.locator('#helper-die').evaluate('(c)=>c.toDataURL()')
      tap(p,'roll');phase(p,'rolling')
      p.wait_for_timeout(270)
      moving=p.locator('#helper-die').evaluate('(c)=>c.toDataURL()')
      assert before!=moving, 'The pixel die did not change frames'
      p.screenshot(path=str(OUT/(engine+'-rolling.png')),full_page=True)
      phase(p,'result');assert 1<=state(p)['result']['main']<=6
      assert 'd20' not in p.locator('body').inner_text().lower()
      tests.append('One-tap start, named players, choices, helper handoff and animated six-sided pixel dice')
      old=state(p);open_game(p);tap(p,'resume');phase(p,'result')
      assert state(p)==old
      tests.append('Reload resumes the exact world, committed dice and remaining chances')
      new_trip(p);second=state(p)
      assert second['w']['name']!=first['w']['name'] and second['w']['sig']!=first['w']['sig']
      assert second['w']['boss']!=first['w']['boss']
      assert not ({x for x in first['route'] if x<28}&{x for x in second['route'] if x<28})
      tests.append('New trip changes setting, cast and mission; previous regular encounter structures are avoided')
      p.emulate_media(reduced_motion='reduce')
      for stop in range(8):
        phase(p,'choose');assert state(p)['stop']==stop
        expected='Moss' if stop%2==0 else 'Wren'
        assert expected+', pick a plan.' in p.locator('#focus').inner_text()
        choices(p,'help' if stop%2==0 else 'safe',stop%3)
        fixed_roll(p);tap(p,'next')
      phase(p,'end');assert state(p)['wins']==8 and len(state(p)['log'])==8
      p.screenshot(path=str(OUT/(engine+'-ending.png')),full_page=True)
      tests.append('Complete eight-stop trip, alternating players and choice-specific ending')
      tap(p,'fresh');phase(p,'choose');choices(p,'safe');fixed_roll(p,1,6)
      assert state(p)['result']['protected'] and state(p)['chances']==5
      tap(p,'next');choices(p,'help');fixed_roll(p,1,1)
      assert not state(p)['result']['success'] and state(p)['chances']==4
      tests.append('Successful backup protects a failed plan; an unprotected failure costs exactly one chance')
      p.evaluate("(()=>{let s=VV.get();s.phase='rolling';s.pending={main:1,helper:1};s.result=null;s.chances=5;s.support='help';localStorage.setItem(VV.key,JSON.stringify(s))})()")
      recovery=c.new_page();open_game(recovery);phase(recovery,'result')
      assert state(recovery)['chances']==4
      open_game(recovery);tap(recovery,'resume');phase(recovery,'result')
      assert state(recovery)['chances']==4
      recovery.close()
      tests.append('Interrupted animation resolves exactly once and cannot duplicate the loss')
      sample=p.evaluate('''()=>{const out=[];for(let i=0;i<150;i++){
        document.querySelector('#menu').click();document.querySelector('[data-global="restart"]').click();
        document.querySelector('[data-do="fresh"]').click();let s=VV.get();
        if(!s||s.phase!=='choose'||!VV.valid(s))throw Error('Invalid generated state');
        if(/\\{(?:n|other|item|where|task|boss)\\}/.test(document.querySelector('#app').innerText))throw Error('Unfilled story token');
        out.push({world:s.w.name,sig:s.w.sig,boss:s.w.boss,person:s.scene.person,route:s.route,need:s.scene.choices.map(c=>c.need)});
      }return out}''')
      for key in ('world','sig','boss','person'): assert len({s[key] for s in sample})==len(sample),key
      assert all(len(set(s['route']))==7 and all(1<=n<=6 for n in s['need']) for s in sample)
      tests.append('150 generated trips: distinct world configurations and named cast, no within-trip repeated structures or missing story tokens')
      for width in (320,390,768):
        p.set_viewport_size({'width':width,'height':844})
        assert p.evaluate('document.documentElement.scrollWidth <= window.innerWidth')
      tests.append('No horizontal overflow at 320, 390 and 768 pixels')
      assert not errors,errors
      tests.append('No uncaught JavaScript errors')
      for mode in ('blocked','corrupt'):
        alt=b.new_context(viewport={'width':390,'height':844},is_mobile=True,has_touch=True,reduced_motion='reduce')
        if mode=='blocked':alt.add_init_script("Object.defineProperty(window,'localStorage',{configurable:true,get(){throw new DOMException('Blocked','SecurityError')}})")
        else:alt.add_init_script("try{localStorage.setItem('vv-pocket-4','{broken');localStorage.setItem('vv-pocket-4-history',JSON.stringify({names:[null,3],worlds:[null,{}],recent:[null]}))}catch(e){}")
        ap=alt.new_page();open_game(ap);start(ap);choices(ap);tap(ap,'roll');phase(ap,'result')
        if mode=='blocked':assert 'Saving is unavailable' in ap.locator('#save-note').inner_text()
        alt.close()
      tests.append('Blocked storage and corrupt old saves cannot prevent starting or playing')
      entry={'browser':engine,'passed':True,'tests':tests,'errors':errors}
    except Exception as exc:
      entry={'browser':engine,'passed':False,'tests':tests,'error':str(exc),'errors':errors}
      try:p.screenshot(path=str(OUT/(engine+'-failure.png')),full_page=True)
      except Exception:pass
    finally:
      report['browsers'].append(entry);(OUT/'report.json').write_text(json.dumps(report,indent=2));print(json.dumps(entry),flush=True);c.close();b.close()
print('PLAY_URL='+PLAY,flush=True)
assert all(x['passed'] for x in report['browsers']),json.dumps(report,indent=2)
