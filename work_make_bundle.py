import base64, hashlib
from pathlib import Path
r=Path(r"F:\CodexWork\tpen-esp\dist\tpen_esp_v33")
def enc(p):
    s=base64.b64encode(p.read_bytes()).decode()
    return "\n".join(s[i:i+76] for i in range(0,len(s),76))
s='''#!/bin/sh
set -eu
D=${1:-./tpen_esp_v33_out}
mkdir -p "$D"
S=$0
awk '/^__P__/{f=1;next}/^__END_P__/{exit}f' "$S"|base64 -d > "$D/p"
awk '/^__D__/{f=1;next}/^__END_D__/{exit}f' "$S"|base64 -d > "$D/classes.dex"
chmod 755 "$D/p"
printf 'extracted: %s\n' "$D"
exit 0
__P__
'''+enc(r/'out/p')+'\n__END_P__\n__D__\n'+enc(r/'out/classes.dex')+'\n__END_D__\n'
o=r/'tpen_esp_v33_payload.sh'; o.write_text(s,encoding='utf-8',newline='\n'); print(o, o.stat().st_size, hashlib.sha256(o.read_bytes()).hexdigest())
