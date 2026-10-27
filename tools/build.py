"""Build the source-only MIT edition; no downloads or emulator launches."""
from pathlib import Path
import argparse
import json
import os
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
TESTS = {
    'engine': ('engine_core', ['lt_engine']),
    'engview': ('engine_view', ['lt_engine_view']),
    'stream': ('stream_core', ['lt_chip', 'lt_audio', 'lt_sfx', 'lt_stream']),
    'smscheck': ('sms_boot_core', ['lt_chip', 'lt_sms_boot']),
    'chip': ('chip_core', ['lt_chip']),
    'audio': ('audio_core', ['lt_chip', 'lt_audio']),
    'sfx': ('sfx_core', ['lt_chip', 'lt_audio', 'lt_sfx']),
    'score': ('score_core', ['lt_chip', 'lt_audio', 'lt_sfx', 'lt_score']),
    'dbscore': ('doebel_score_core', ['lt_chip', 'lt_audio', 'lt_score']),
    'graphics': ('gfx_host', ['lt_gfx', 'lt_gfx_rows']),
    'surface': ('surface_core', ['lt_surface']),
    'time': ('time_core', ['lt_time']),
    'event': ('event_core', ['lt_event']),
    'keys': ('keys_core', ['lt_keys']),
    'blit16': ('gfx_blit16', ['lt_gfx', 'lt_gfx_rows']),
}
# DOS-only demos of the platform services: name -> sources.
DOS_DEMOS = {
    'ENGDOS': ['tests/engine_dos.c', 'src/lt_engine_view.c',
               'adapters/lt_engine_dos.c'],
    'ENGDEMO': ['examples/engine_demo.c', 'src/lt_engine.c',
                'src/lt_engine_view.c', 'adapters/lt_engine_dos.c',
                'src/lt_time.c', 'adapters/lt_time_dos.c'],
    'TXTDEMO': ['examples/engine_text_demo.c', 'src/lt_engine.c',
                'src/lt_engine_view.c', 'adapters/lt_engine_dos.c',
                'src/lt_time.c', 'adapters/lt_time_dos.c'],
    'STRMDEMO': ['examples/stream_demo.c', 'src/lt_chip.c', 'src/lt_audio.c',
                 'src/lt_sfx.c', 'src/lt_stream.c', 'adapters/lt_dos.c'],
    'STRMBEN': ['tests/stream_bench.c', 'src/lt_chip.c', 'src/lt_audio.c',
                'src/lt_sfx.c', 'src/lt_stream.c'],
    'SMSBOOT': ['examples/sms_demo.c', 'src/lt_chip.c',
                'src/lt_sms_boot.c', 'adapters/lt_dos.c', 'adapters/lt_tick_dos.c'],
    'DOEBEL': ['examples/doebel_demo.c', 'src/lt_chip.c',
               'src/lt_audio.c', 'src/lt_score.c', 'adapters/lt_dos.c',
               'adapters/lt_tick_dos.c'],
    'PAGEDEMO': ['examples/page_demo.c', 'src/lt_chip.c', 'src/lt_gfx.c',
                 'src/lt_event.c', 'adapters/lt_dos.c',
                 'adapters/lt_input_dos.c', 'adapters/lt_dos_app.c',
                 'adapters/lt_dos_app_dos.c', 'adapters/lt_page_dos.c'],
    'KEYDEMO': ['examples/keys_demo.c', 'src/lt_chip.c', 'src/lt_gfx.c',
                'src/lt_keys.c', 'adapters/lt_dos.c',
                'adapters/lt_dos_app.c', 'adapters/lt_dos_app_dos.c',
                'adapters/lt_keys_dos.c'],
    'TICKDEMO': ['examples/tick_demo.c', 'src/lt_chip.c', 'src/lt_gfx.c',
                 'src/lt_audio.c', 'src/lt_keys.c', 'adapters/lt_dos.c',
                 'adapters/lt_dos_app.c', 'adapters/lt_dos_app_dos.c',
                 'adapters/lt_keys_dos.c', 'adapters/lt_tick_dos.c'],
}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--target', choices=('host', 'dos'), required=True)
    parser.add_argument('--cc', default='cc')
    parser.add_argument('--gcc-launcher', type=Path)
    parser.add_argument('--runtime-bin', type=Path)
    parser.add_argument('--watcom', type=Path, default=Path('C:/WATCOM'))
    args = parser.parse_args()
    if args.target == 'dos' and (args.gcc_launcher or args.runtime_bin):
        parser.error('--gcc-launcher and --runtime-bin apply to host builds')
    if args.runtime_bin and not args.gcc_launcher:
        parser.error('--runtime-bin requires --gcc-launcher')
    if set(TESTS) & {name.lower() for name in DOS_DEMOS}:
        raise RuntimeError('DOS test/demo executable names collide')
    out = ROOT / 'build' / args.target
    out.mkdir(parents=True, exist_ok=True)
    env = dict(os.environ)
    records = []

    def run(argv, label):
        run_env = env
        if label.endswith(' run') and args.runtime_bin:
            run_env = dict(env)
            run_env['PATH'] = str(args.runtime_bin.resolve()) + os.pathsep + \
                run_env.get('PATH', '')
        proc = subprocess.Popen(list(map(str, argv)), cwd=out, env=run_env,
                                stdout=subprocess.PIPE,
                                stderr=subprocess.PIPE, text=True)
        try:
            stdout, stderr = proc.communicate(timeout=30)
        except subprocess.TimeoutExpired:
            if os.name == 'nt':
                subprocess.run(['taskkill', '/PID', str(proc.pid), '/T', '/F'],
                               capture_output=True, timeout=5)
            else:
                proc.kill()
            proc.communicate(timeout=5)
            raise RuntimeError(label + ': compiler exceeded 30 seconds')
        result = subprocess.CompletedProcess(argv, proc.returncode,
                                             stdout, stderr)
        records.append(dict(label=label, argv=list(map(str, argv)),
                            returncode=result.returncode,
                            stdout=result.stdout, stderr=result.stderr))
        (out / 'BUILD.json').write_text(
            json.dumps(records, indent=2) + '\n', encoding='utf-8')
        if result.returncode:
            raise RuntimeError(label + ': ' + result.stdout + result.stderr)
        return result

    run([sys.executable, ROOT / 'examples/doebel/generate.py', '--check'],
        'doebel score data')

    if args.target == 'dos':
        watcom = args.watcom.resolve()
        if os.name == 'nt':
            env.update(WATCOM=str(watcom), INCLUDE=str(watcom / 'H'),
                       LIB=str(watcom / 'LIB286') + ';' +
                           str(watcom / 'LIB286/DOS'))
            cc = watcom / 'BINNT/wcl.exe'
        else:
            # Open Watcom's Linux host layout (lower-case directories)
            env.update(WATCOM=str(watcom), INCLUDE=str(watcom / 'h'),
                       LIB=str(watcom / 'lib286') + os.pathsep +
                           str(watcom / 'lib286/dos'),
                       PATH=str(watcom / 'binl') + os.pathsep +
                           env.get('PATH', ''))
            cc = watcom / 'binl/wcl'
        flags = [cc, '-q', '-bt=dos', '-0', '-ms', '-ecc', '-os', '-s',
                 '-w4', '-wx', '-i=' + str(ROOT / 'include')]
        for source in sorted((ROOT / 'src').glob('*.c')) + sorted(
                (ROOT / 'adapters').glob('*.c')):
            run([*flags, '-c', source], source.stem)
        run([*flags, '-k4096', '-fe=DEMO.EXE',
             ROOT / 'examples/dos_demo.c', ROOT / 'src/lt_chip.c',
             ROOT / 'src/lt_gfx.c', ROOT / 'src/lt_event.c',
             ROOT / 'src/lt_time.c', ROOT / 'adapters/lt_dos.c',
             ROOT / 'adapters/lt_input_dos.c',
             ROOT / 'adapters/lt_time_dos.c',
             ROOT / 'adapters/lt_dos_app.c',
             ROOT / 'adapters/lt_dos_app_dos.c'], 'demo')
        run([*flags, '-k4096', '-fe=SFXDEMO.EXE',
             ROOT / 'examples/dos_sfx_demo.c', ROOT / 'src/lt_chip.c',
             ROOT / 'src/lt_audio.c', ROOT / 'src/lt_sfx.c',
             ROOT / 'src/lt_time.c', ROOT / 'adapters/lt_dos.c',
             ROOT / 'adapters/lt_time_dos.c'], 'sfx demo')
        run([*flags, '-dSMS_LOGO', '-k4096', '-fe=TANDYLOG.EXE',
             ROOT / 'examples/sms_demo.c', ROOT / 'src/lt_chip.c',
             ROOT / 'src/lt_sms_boot.c', ROOT / 'src/lt_gfx.c',
             ROOT / 'src/lt_gfx_rows.c', ROOT / 'adapters/lt_dos.c',
             ROOT / 'adapters/lt_tick_dos.c'], 'Tandy logo')
        for exe, sources in DOS_DEMOS.items():
            run([*flags, '-k4096', '-fe=' + exe + '.EXE',
                 *[ROOT / src for src in sources]], exe.lower())
    for name, (test, modules) in TESTS.items():
        sources = [ROOT / 'src' / (module + '.c') for module in modules]
        sources.append(ROOT / 'tests' / (test + '.c'))
        if args.target == 'dos':
            run([*flags, '-k4096', '-fe=' + name + '.EXE', *sources], name)
        else:
            program = out / (name + ('.exe' if os.name == 'nt' else ''))
            compiler = [args.cc]
            if args.gcc_launcher:
                compiler = [sys.executable,
                            str(args.gcc_launcher.resolve()), '--']
            run([*compiler, '-std=c89', '-pedantic-errors', '-Wall', '-Wextra',
                 '-Werror', '-O2', '-I' + str(ROOT / 'include'), *sources,
                 '-o', program], name + ' compile')
            result = run([program], name + ' run')
            print(result.stdout.strip())
    if args.target == 'dos':
        run([*flags, '-k4096', '-fe=DOSAPP.EXE',
             ROOT / 'tests/dos_app_dos.c', ROOT / 'src/lt_chip.c',
             ROOT / 'src/lt_gfx.c', ROOT / 'adapters/lt_dos.c',
             ROOT / 'adapters/lt_dos_app.c',
             ROOT / 'adapters/lt_dos_app_dos.c'], 'dos_app')
    else:
        program = out / ('dos_app_host.exe' if os.name == 'nt'
                         else 'dos_app_host')
        sources = [ROOT / 'src/lt_chip.c', ROOT / 'src/lt_gfx.c',
                   ROOT / 'adapters/lt_dos_app.c',
                   ROOT / 'tests/dos_app_host.c',
                   ROOT / 'tests/dos_app_platform_fake.c']
        compiler = [args.cc]
        if args.gcc_launcher:
            compiler = [sys.executable,
                        str(args.gcc_launcher.resolve()), '--']
        run([*compiler, '-std=c89', '-pedantic-errors', '-Wall', '-Wextra',
             '-Werror', '-O2', '-I' + str(ROOT / 'include'), *sources,
             '-o', program], 'dos_app compile')
        result = run([program], 'dos_app run')
        print(result.stdout.strip())
    print('PASS ' + args.target + (' build only' if args.target == 'dos'
                                   else ' build and host tests'))


if __name__ == '__main__':
    main()
