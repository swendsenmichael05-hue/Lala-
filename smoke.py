"""Test the public play URL, not an HTML file or a mocked response."""
import json
import pathlib
import time
import requests
from playwright.sync_api import sync_playwright

RAW = 'https://raw.githubusercontent.com/swendsenmichael05-hue/Lala-/veil-vow-browser/index.html'
PLAY = 'https://htmlpreview.github.io/?' + RAW
OUT = pathlib.Path('test-output')
OUT.mkdir(exist_ok=True)
report = {'play_url': PLAY, 'physical_device': False, 'browsers': []}

for attempt in range(12):
    response = requests.get(RAW, timeout=30)
    if response.status_code == 200 and 'browser edition · 3.2' in response.text:
        break
    time.sleep(5)
else:
    raise RuntimeError('The published game source did not become available.')
assert response.text == pathlib.Path('index.html').read_text(), 'Public source differs from this build'


def phase(page, expected):
    page.wait_for_function('(expected) => window.VV && window.VV.get() && window.VV.get().phase === expected', arg=expected, timeout=25000)
    assert page.locator('#game-shell').is_visible(), 'The game displayed its error screen'


def open_game(page):
    page.goto(PLAY, wait_until='domcontentloaded', timeout=90000)
    page.wait_for_function("document.documentElement.dataset.gameReady === 'true'", timeout=60000)
    assert page.locator('#build').inner_text() == 'browser edition · 3.2'


def tap(page, action):
    page.locator('#app button[data-a="' + action + '"]').tap(timeout=15000)


def start(page):
    tap(page, 'fresh')
    phase(page, 'intro')
    tap(page, 'begin')
    phase(page, 'act')


def choices(page, support='help0'):
    tap(page, 'pick0')
    phase(page, 'help')
    tap(page, support)
    phase(page, 'ready')


def force_roll(page, a=20, b=20):
    page.evaluate('''([a,b]) => {
      window.__oldCrypto = window.crypto.getRandomValues;
      let i=0;
      Object.defineProperty(window.crypto, 'getRandomValues', {configurable:true,value:(arr)=>{
        arr[0]=([a-1,b-1][i++] ?? 19); return arr;
      }});
    }''', [a,b])
    tap(page, 'roll')
    phase(page, 'result')
    page.evaluate("Object.defineProperty(window.crypto,'getRandomValues',{configurable:true,value:window.__oldCrypto}); delete window.__oldCrypto;")

with sync_playwright() as p:
    for browser_name in ('chromium','webkit'):
        tests=[]
        errors=[]
        browser=getattr(p,browser_name).launch()
        context=browser.new_context(viewport={'width':390,'height':844},is_mobile=True,has_touch=True,device_scale_factor=2)
        page=context.new_page()
        page.on('pageerror',lambda e: errors.append(str(e)))
        try:
            open_game(page)
            start(page)
            first=page.evaluate('window.VV.get()')
            tests.append('Start and first encounter respond to actual touch events')
            page.screenshot(path=str(OUT/(browser_name+'-encounter.png')),full_page=True)
            choices(page)
            tap(page,'roll')
            phase(page,'result')
            tests.append('Normal-motion dice animation completes and enables Next')
            page.screenshot(path=str(OUT/(browser_name+'-result.png')),full_page=True)
            old=page.evaluate('window.VV.get()')
            open_game(page)
            tap(page,'resume')
            phase(page,'result')
            resumed=page.evaluate('window.VV.get()')
            assert resumed['w']==old['w'] and resumed['result']==old['result'] and resumed['luck']==old['luck']
            tests.append('Reload and Continue preserve the world, roll and luck')
            page.locator('#restart').tap()
            tap(page,'fresh')
            phase(page,'intro')
            next_world=page.evaluate('window.VV.get()')
            assert next_world['w']['name'] != first['w']['name']
            assert next_world['w']['recipient'] != first['w']['recipient']
            assert not set(next_world['route']) & set(first['route'])
            tests.append('Next run changes world and cast and avoids previous route structures')
            tap(page,'begin')
            phase(page,'act')
            page.emulate_media(reduced_motion='reduce')
            for stop in range(8):
                phase(page,'act')
                assert page.evaluate('window.VV.get().stop')==stop
                choices(page,'help1' if stop%2 else 'help0')
                force_roll(page)
                tap(page,'next')
            phase(page,'end')
            assert page.evaluate('window.VV.get().wins')==8
            tests.append('Complete eight-encounter run with deterministic successful dice')
            tap(page,'fresh')
            phase(page,'intro')
            tap(page,'begin')
            phase(page,'act')
            choices(page,'help1')
            force_roll(page,1,20)
            r=page.evaluate('window.VV.get().result')
            assert not r['success'] and r['safe'] and r['delta']==0
            tests.append('Natural 1 fails; successful escape protects luck')
            # Restore a genuine committed roll as if the tab closed during animation.
            page.evaluate('''() => {let s=window.VV.get();s.phase='rolling';s.pending={a:1,b:1};s.help=0;s.luck=6;s.result=null;localStorage.setItem('vv-browser-3',JSON.stringify(s));}''')
            # Do not let the existing page's pagehide save overwrite the fixture.
            recovery=context.new_page()
            recovery.on('pageerror',lambda e:errors.append(str(e)))
            open_game(recovery)
            phase(recovery,'result')
            assert recovery.evaluate('window.VV.get().luck')==4
            open_game(recovery)
            tap(recovery,'resume')
            assert recovery.evaluate('window.VV.get().luck')==4
            tests.append('Interrupted roll resolves once without duplicate luck loss')
            recovery.close()
            for width in (320,390,768):
                page.set_viewport_size({'width':width,'height':844})
                assert page.evaluate('document.documentElement.scrollWidth <= window.innerWidth'), 'Horizontal overflow'
            tests.append('No horizontal overflow at 320, 390 and 768 pixels')
            assert not errors, errors
            tests.append('No uncaught browser JavaScript errors')
            # A separate context denies storage from the start.
            blocked=browser.new_context(viewport={'width':390,'height':844},is_mobile=True,has_touch=True,reduced_motion='reduce')
            blocked.add_init_script("Object.defineProperty(window,'localStorage',{configurable:true,get(){throw new DOMException('Blocked','SecurityError')}});")
            bp=blocked.new_page()
            open_game(bp)
            start(bp)
            choices(bp)
            tap(bp,'roll')
            phase(bp,'result')
            assert 'cannot save' in bp.locator('#notice').inner_text()
            blocked.close()
            tests.append('Storage denial does not block starting, choices or dice')
            entry={'browser':browser_name,'passed':True,'tests':tests,'errors':errors}
        except Exception as exc:
            entry={'browser':browser_name,'passed':False,'tests':tests,'error':str(exc),'errors':errors}
            try: page.screenshot(path=str(OUT/(browser_name+'-failure.png')),full_page=True)
            except Exception: pass
        finally:
            report['browsers'].append(entry)
            (OUT/'report.json').write_text(json.dumps(report,indent=2))
            print(json.dumps(entry),flush=True)
            context.close()
            browser.close()

print('PLAY_URL='+PLAY,flush=True)
print(json.dumps(report,indent=2),flush=True)
assert all(x['passed'] for x in report['browsers']), 'One or more public browser checks failed'
