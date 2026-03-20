import sys, random, re

pattern = re.compile(r'\[surge-rs [0-9.]*\]')
matches = ['[SRS]', 'Downloaded', 'Fresh', 'Compiling', 'Building', 'Finished']
progress = ".:/\\o-^#|\"!$%&=*"

for l in sys.stdin:
    l = l.lstrip().rstrip().replace('\x1b[A', '')
    if "could not compile" in l:
        print('\x1b[1;31m')
        print('#############################')
        print('#YO! ERROR! HEED MY WARNING!#')
        print('#############################')
        print('\x1b[0m\x1b[1m', end='')
        print('There has been an error when building surge-rs.')
        print('This filter obscures thousands of lines of garbage, which is a net positive.')
        print('...It also obscures a good chunk of the error.')
        print()
        print('To debug this error, go ahead and disable this filter. Or give me a call. ', end='')
        print('\x1b[1;31mGood luck! (-:\x1b[1m')
        print('----------------------------------------------------------------------------------------\x1b[0m')
        print(l)
    elif ("error:" in l or "process didn't" in l) and "Running" not in l:
        print('\x1b[1;31m ERR!!! => '+l+'\x1b[0m')
    elif any(m in l for m in matches):
        print("\r[OK!]"+pattern.sub('', l))
    #elif 'Running' in l:
    else:
        print('\r[' + ''.join(random.choices(progress, k=3)) + ']', end='')
