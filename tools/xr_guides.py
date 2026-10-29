"""Generate original spatial diagrams and ten localized, offline XR guides."""
from pathlib import Path
import argparse
import json
from html import escape

ROOT = Path(__file__).resolve().parents[1]
TEXT = {
'status': [
'Experimental preview. Compilation, lint and automated geometry/input checks are performed. No headset has been connected, installed or used for this version; tracking, comfort, passthrough and frame pacing await your physical test.',
'Aperçu expérimental. Compilation, lint et contrôles automatisés de géométrie et d’entrée sont effectués. Aucun casque n’a été connecté, installé ou utilisé pour cette version ; suivi, confort, passthrough et fluidité attendent ton essai physique.',
'Vista experimental. Se comprueban compilación, lint, geometría y entrada. No se ha conectado, instalado ni usado un visor para esta versión; seguimiento, comodidad, passthrough y fluidez esperan tu prueba física.',
'実験的プレビューです。コンパイル、lint、形状と入力の自動テストを行います。この版ではヘッドセットへの接続、インストール、実機テストを行っていません。追跡、快適さ、パススルー、表示間隔は実機確認が必要です。',
'Experimentelle Vorschau. Kompilierung, Lint sowie Geometrie und Eingaben werden automatisch geprüft. Für diese Version wurde kein Headset verbunden, installiert oder benutzt. Tracking, Komfort, Passthrough und Bildrhythmus warten auf deinen Praxistest.',
'Anteprima sperimentale. Si verificano compilazione, lint, geometria e input. Nessun visore è stato collegato, installato o usato per questa versione; tracciamento, comfort, passthrough e fluidità attendono la prova fisica.',
'Prévia experimental. Compilação, lint, geometria e entrada são verificados. Nenhum headset foi conectado, instalado ou usado nesta versão; rastreamento, conforto, passthrough e fluidez aguardam seu teste físico.',
'Экспериментальная версия. Проверяются сборка, lint, геометрия и ввод. Для этой версии шлем не подключался, установка и физическое тестирование не проводились. Отслеживание, комфорт, сквозной режим и плавность требуют проверки на устройстве.',
'实验性预览。执行编译、lint、几何和输入自动检查。本版未连接、安装或使用任何头显；跟踪、舒适度、透视和帧间隔仍待实机测试。',
'실험적 미리보기입니다. 컴파일, lint, 기하와 입력 자동 검사를 수행합니다. 이 버전은 헤드셋 연결, 설치, 실기 테스트를 하지 않았습니다. 추적, 편안함, 패스스루 및 프레임 간격은 실기 확인이 필요합니다.'
],
'install': [
'For a later test, enable developer mode and install the public ARM64 APK with SideQuest or adb install -r. Open Mario Compilation Ultra XR in Unknown Sources, choose a cartridge or ZIP using the system picker, then Play. Import All-Stars + World and Yoshi’s Island separately. Public APKs contain no ROMs or progress.',
'Pour ton essai ultérieur, active le mode développeur et installe l’APK ARM64 public avec SideQuest ou adb install -r. Ouvre Mario Compilation Ultra XR dans Sources inconnues, choisis une cartouche ou un ZIP avec le sélecteur système, puis Jouer. Importe All-Stars + World et Yoshi’s Island séparément. L’APK public ne contient ni ROM ni progression.',
'Para probar más tarde, activa el modo desarrollador e instala el APK ARM64 público con SideQuest o adb install -r. Abre Mario Compilation Ultra XR en Fuentes desconocidas, elige un cartucho o ZIP con el selector y pulsa Jugar. Importa All-Stars + World y Yoshi’s Island por separado. El APK público no contiene ROMs ni progreso.',
'後日テストする際は開発者モードを有効にし、SideQuestまたはadb install -rで公開ARM64 APKをインストールしてください。提供元不明からMario Compilation Ultra XRを開き、システム選択画面でカートリッジまたはZIPを選んでプレイします。All-Stars + WorldとYoshi’s Islandは別々に読み込みます。公開APKにROMや進行データはありません。',
'Für den späteren Test Entwicklermodus aktivieren und die öffentliche ARM64-APK mit SideQuest oder adb install -r installieren. Mario Compilation Ultra XR unter Unbekannte Quellen öffnen, eine Cartridge oder ZIP im Systemdialog wählen und starten. All-Stars + World und Yoshi’s Island separat importieren. Die öffentliche APK enthält keine ROMs oder Spielstände.',
'Per il test futuro, attiva la modalità sviluppatore e installa l’APK ARM64 pubblico con SideQuest o adb install -r. Apri Mario Compilation Ultra XR da Origini sconosciute, scegli una cartuccia o ZIP e premi Gioca. Importa All-Stars + World e Yoshi’s Island separatamente. L’APK pubblico non contiene ROM né progressi.',
'Para testar depois, ative o modo de desenvolvedor e instale o APK ARM64 público com SideQuest ou adb install -r. Abra Mario Compilation Ultra XR em Fontes desconhecidas, escolha cartucho ou ZIP pelo seletor do sistema e jogue. Importe All-Stars + World e Yoshi’s Island separadamente. O APK público não contém ROMs nem progresso.',
'Для последующего теста включите режим разработчика и установите публичный ARM64 APK через SideQuest или adb install -r. Откройте Mario Compilation Ultra XR в неизвестных источниках, выберите картридж или ZIP системным диалогом и начните игру. All-Stars + World и Yoshi’s Island импортируются отдельно. Публичный APK не содержит ROM и сохранений.',
'稍后测试时，启用开发者模式并通过 SideQuest 或 adb install -r 安装公开 ARM64 APK。在未知来源中打开 Mario Compilation Ultra XR，用系统选择器导入卡带或 ZIP 后开始游戏。All-Stars + World 和 Yoshi’s Island 须分别导入。公开 APK 不含 ROM 或进度。',
'나중에 시험할 때 개발자 모드를 켜고 SideQuest 또는 adb install -r로 공개 ARM64 APK를 설치하세요. 알 수 없는 출처에서 Mario Compilation Ultra XR을 열고 시스템 선택기로 카트리지나 ZIP을 고른 후 플레이하세요. All-Stars + World와 Yoshi’s Island는 별도로 가져옵니다. 공개 APK에는 ROM이나 진행 정보가 없습니다.'
],
'placement': [
'Select the level, native CRT, remote or options as the placement target. One grip moves/rotates it; two grips resize the level even while position is locked. Use the remote’s fine controls or half/double/reset actions as alternatives. Recover brings the interface back nearby. Y opens options; the Play page has all SNES buttons.',
'Choisis le niveau, le CRT natif, la télécommande ou les options comme cible. Une poignée le déplace et l’oriente ; deux poignées redimensionnent le niveau même si sa position est verrouillée. La télécommande propose aussi déplacement précis et moitié/double/réinitialiser. Récupérer ramène l’interface à proximité. Y ouvre les options ; la page Jouer comporte toutes les touches SNES.',
'Elige el nivel, CRT nativo, mando u opciones como objetivo. Una empuñadura mueve y gira; ambas cambian el tamaño aun con la posición bloqueada. El mando ofrece ajustes precisos y mitad/doble/restablecer. Recuperar acerca la interfaz. Y abre opciones; la página Jugar incluye todos los botones SNES.',
'レベル、ネイティブCRT、リモコン、設定を配置対象にします。片手のグリップで移動と回転、両グリップで位置ロック中もレベルを拡大縮小できます。リモコンには微調整、半分、2倍、リセットもあります。復帰操作でUIを近くに戻せます。Yで設定、プレイページには全SNESボタンがあります。',
'Level, natives CRT, Fernbedienung oder Optionen als Ziel wählen. Ein Griff verschiebt/dreht; zwei skalieren das Level auch bei gesperrter Position. Feinsteuerung und Halbieren/Verdoppeln/Zurücksetzen sind ebenfalls verfügbar. Wiederherstellen holt die Oberfläche zurück. Y öffnet Optionen; die Spielseite enthält alle SNES-Tasten.',
'Seleziona livello, CRT nativo, telecomando o opzioni. Una presa sposta e ruota; due ridimensionano il livello anche con posizione bloccata. Sono disponibili regolazioni fini e metà/doppio/ripristina. Recupera riporta l’interfaccia vicino. Y apre le opzioni; la pagina Gioca contiene tutti i pulsanti SNES.',
'Escolha nível, CRT nativo, controle ou opções como alvo. Uma empunhadura move e gira; duas redimensionam o nível mesmo com posição travada. Há ajustes finos e metade/dobro/redefinir. Recuperar traz a interface de volta. Y abre opções; a página Jogar tem todos os botões SNES.',
'Выберите уровень, исходный CRT, пульт или настройки. Один захват перемещает и вращает, два меняют размер уровня даже при фиксации позиции. Есть точная настройка, половина/двойной размер/сброс. Возврат приближает интерфейс. Y открывает настройки; игровая страница содержит все кнопки SNES.',
'将关卡、原生 CRT、遥控器或选项设为目标。单手握持可移动旋转，双手可在位置锁定时缩放关卡。遥控器还提供微调、减半、加倍和重置。恢复按钮将界面带回附近。Y 打开选项，游玩页面包含所有 SNES 按钮。',
'레벨, 원래 CRT, 리모컨 또는 옵션을 대상으로 고릅니다. 한 그립으로 이동·회전하고 두 그립으로 위치 잠금 중에도 레벨 크기를 바꿉니다. 미세 조정, 절반, 두 배, 초기화도 가능합니다. 복구로 화면을 가까이 되돌립니다. Y는 옵션을 열며 플레이 페이지에 모든 SNES 버튼이 있습니다.'
],
'room': [
'New Quest data requests 90 Hz and a transparent two-metre AR mural. Supported refresh falls back when unavailable; native simulation speed is unchanged. Scene/anchor permissions and a room scan are optional for automatic walls. Manual placement and floor calibration remain available. Passthrough is supplied by the runtime; the app requests no Internet, microphone or camera-image permission.',
'Une nouvelle installation demande 90 Hz et une fresque AR transparente de deux mètres. La fréquence utilise un repli si nécessaire ; la simulation garde sa cadence native. Les permissions de scène/ancrage et le scan de pièce sont facultatifs pour les murs automatiques. Placement manuel et calibration du sol restent accessibles. Le runtime fournit le passthrough ; l’application ne demande ni Internet, ni micro, ni images caméra.',
'Una instalación nueva solicita 90 Hz y un mural AR transparente de dos metros. Si no está disponible se usa otra frecuencia; la simulación conserva su ritmo. Permisos de escena/anclas y escaneo son opcionales para paredes automáticas. Hay colocación manual y calibración del suelo. El runtime compone passthrough; la app no solicita Internet, micrófono ni imágenes de cámara.',
'新規インストールは90 Hzと高さ2 mの透明AR壁画を既定にします。非対応の場合は別の表示周波数を選び、ゲーム本来の速度は変えません。壁の自動配置には任意のシーン・アンカー権限と室内スキャンを使います。手動配置と床調整も可能です。パススルーはランタイムが提供し、ネット、マイク、カメラ画像の権限は要求しません。',
'Neue Installationen fordern 90 Hz und ein transparentes zwei Meter hohes AR-Wandbild an. Nicht unterstützte Raten fallen zurück; die Simulation behält ihr Originaltempo. Raumscan und Szene-/Ankerrechte sind für automatische Wände optional. Manuelle Platzierung und Bodenkalibrierung bleiben verfügbar. Passthrough liefert die Laufzeit; keine Internet-, Mikrofon- oder Kamerabildrechte.',
'Una nuova installazione richiede 90 Hz e un murale AR trasparente alto due metri. Se necessario si usa una frequenza supportata; la simulazione resta originale. Scansione stanza e permessi scena/ancore sono facoltativi per le pareti automatiche. Restano posizione manuale e calibrazione del pavimento. Il runtime fornisce passthrough, senza permessi Internet, microfono o immagini della fotocamera.',
'Uma nova instalação pede 90 Hz e um mural AR transparente de dois metros. Há frequência alternativa compatível; a simulação mantém o ritmo original. Varredura da sala e permissões de cena/âncoras são opcionais para paredes automáticas. Posicionamento manual e calibração do chão continuam disponíveis. O runtime fornece passthrough; sem permissões de Internet, microfone ou imagens da câmera.',
'Новая установка запрашивает 90 Гц и прозрачную AR-панораму высотой два метра. При необходимости выбирается доступная частота; темп игры остаётся исходным. Для автоматических стен необязательны скан комнаты и разрешения сцен/якорей. Есть ручное размещение и калибровка пола. Сквозное изображение предоставляет среда; нет разрешений Интернета, микрофона или кадров камеры.',
'新安装默认请求 90 Hz 和两米高的透明 AR 壁画。不支持时回退到可用刷新率，游戏模拟保持原速。自动墙面可选用场景、锚点权限及房间扫描；也支持手动放置和地面校准。透视由运行时提供，应用不请求网络、麦克风或摄像头画面权限。',
'새 설치는 90 Hz와 높이 2m의 투명 AR 벽화를 요청합니다. 지원하지 않으면 가능한 주사율로 전환하며 게임 속도는 그대로입니다. 자동 벽 배치에 선택적 장면·앵커 권한과 방 스캔을 씁니다. 수동 배치와 바닥 보정도 가능합니다. 패스스루는 런타임이 제공하며 인터넷, 마이크, 카메라 영상 권한을 요청하지 않습니다.'
],
'desktop': [
'Desktop quick controls: hold right-stick click, then D-pad left/right for zoom, up for 1:1, down for the whole map, A to freeze, B for the original camera, X for a screenshot, Y to follow the player. Releasing the modifier restores normal SNES controls. F1 or the menu’s Y help opens an illustrated setting explanation.',
'Commandes rapides sur ordinateur : maintiens le clic du stick droit, puis croix gauche/droite pour le zoom, haut pour 1:1, bas pour la carte entière, A pour figer, B pour la caméra originale, X pour une capture, Y pour suivre le joueur. Relâcher rend les commandes SNES normales. F1 ou l’aide Y du menu ouvre une explication illustrée.',
'Controles rápidos en PC: mantén pulsado el stick derecho; cruceta izquierda/derecha cambia zoom, arriba 1:1, abajo mapa completo, A fija, B cámara original, X captura, Y sigue al jugador. Al soltar vuelven los controles SNES. F1 o ayuda Y del menú abre la explicación ilustrada.',
'PCのクイック操作：右スティック押し込み中、十字左右でズーム、上で1:1、下で全体マップ、Aで固定、Bで元カメラ、Xで撮影、Yでプレイヤー追従。離すと通常のSNES操作に戻ります。F1またはメニューのYヘルプで図解を開きます。',
'PC-Schnellsteuerung: rechten Stick gedrückt halten; Steuerkreuz links/rechts zoomt, oben 1:1, unten ganze Karte, A fixiert, B Originalkamera, X Screenshot, Y Spieler verfolgen. Loslassen stellt SNES-Steuerung wieder her. F1 oder Y-Hilfe im Menü öffnet die bebilderte Erklärung.',
'Comandi rapidi PC: tieni premuto lo stick destro; croce sinistra/destra zoom, su 1:1, giù mappa intera, A blocca, B camera originale, X schermata, Y segue il giocatore. Rilascia per tornare ai comandi SNES. F1 o aiuto Y nel menu apre la spiegazione illustrata.',
'Atalhos no PC: segure o clique do analógico direito; direcional esquerda/direita muda zoom, cima 1:1, baixo mapa inteiro, A trava, B câmera original, X captura, Y segue o jogador. Soltar restaura os controles SNES. F1 ou ajuda Y no menu abre a explicação ilustrada.',
'Быстрые команды ПК: удерживайте нажатие правого стика; крестовина влево/вправо — масштаб, вверх — 1:1, вниз — вся карта, A — фиксация, B — исходная камера, X — снимок, Y — следить за игроком. Отпускание возвращает SNES-ввод. F1 или справка Y в меню открывает иллюстрацию.',
'电脑快捷控制：按住右摇杆，方向键左右缩放，上为 1:1，下为全图，A 固定，B 原镜头，X 截图，Y 跟随玩家。松开后恢复 SNES 操作。F1 或菜单 Y 帮助打开图解。',
'PC 빠른 조작: 오른쪽 스틱을 누른 채 방향키 좌우로 확대·축소, 위는 1:1, 아래는 전체 지도, A 고정, B 원래 카메라, X 캡처, Y 플레이어 추적입니다. 놓으면 SNES 조작으로 복귀합니다. F1 또는 메뉴 Y 도움말로 그림 설명을 엽니다.'
]}

def main():
    parser=argparse.ArgumentParser();parser.add_argument('--check',action='store_true');args=parser.parse_args()
    languages=json.loads((ROOT/'assets/locales/languages.json').read_text(encoding='utf8'))
    outputs={}
    for i,language in enumerate(languages):
        code=language['code'];catalog=json.loads((ROOT/f'assets/locales/{code}.json').read_text(encoding='utf8'))
        tr=lambda key:catalog[key]
        content=[tr('questintro'),TEXT['status'][i],TEXT['install'][i],TEXT['placement'][i],tr('remote_hand_hint'),TEXT['room'][i],tr('Preview help detail'),tr('questpinballnote'),TEXT['desktop'][i]]
        modes=['questscreenmode','questwallfit','questmural','questflooratlas','questpinball','questcrtroom','questcurved']
        nav=' · '.join(f"[{lang['name']}](../{lang['code']}/XR.md)" for lang in languages)
        md=f'# Mario Compilation Ultra 0.4.0 · {tr("questtab")}\n\n{nav}\n\n![{tr("questtab")}](xr.svg)\n\n'
        md+='\n\n'.join(content[:3])+'\n\n'
        md+=' | '.join(tr(k) for k in modes)+'\n\n'+'\n\n'.join(content[3:])+'\n\n'
        md+='SNES: D-pad · A · B · X · Y · L · R · Start · Select\n\n'
        mapping='Touch: A → SNES B · B → SNES Y · X → SNES A · RT → SNES X · R3 → Start · LT → Rewind · Y → '+tr('remote_options')
        md+=mapping+'\n\n'
        md+='[HTML](xr.html) · [Guide](README.md) · [Build](../BUILD.md) · [Parity](../PARITY.md) · [Validation](../TEST_REPORT.md)\n\n'
        md+='[Meta developer mode](https://developers.meta.com/vr/documentation/android-apps/enable-developer-mode/) · [Meta refresh API](https://developers.meta.com/vr/documentation/native/android/mobile-display-refresh-rate/) · [Meta passthrough](https://developers.meta.com/vr/documentation/native/android/mobile-passthrough/)\n'
        folder=ROOT/'docs'/code;folder.mkdir(exist_ok=True)
        outputs[folder/'XR.md']=md
        h=escape
        html=f'<!doctype html><html lang="{code}"><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>Mario Compilation Ultra XR</title><style>@font-face{{font-family:MCU;src:url(../../assets/Interface.otf)}}body{{background:#0d1620;color:#e5eff2;font:18px/1.7 MCU,system-ui;max-width:1050px;margin:auto;padding:28px}}a{{color:#98d6d8}}img{{width:100%;height:auto}}li{{padding:8px}}p{{padding:12px;background:#162434;border-radius:8px}}h1{{color:#abd8bc}}</style><h1>Mario Compilation Ultra 0.4.0 · {h(tr("questtab"))}</h1><a href="guide.html">{h(tr("About"))}</a><img src="xr.svg" alt="{h(tr("questtab"))}">'
        html+=''.join(f'<p>{h(p)}</p>' for p in content[:3])+'<ul>'+''.join(f'<li>{h(tr(k))}</li>' for k in modes)+'</ul>'+''.join(f'<p>{h(p)}</p>' for p in content[3:])
        html+='<p>SNES: D-pad · A · B · X · Y · L · R · Start · Select</p><p>'+h(mapping)+'</p><a href="../PARITY.md">Parity</a> · <a href="../TEST_REPORT.md">Validation</a></html>\n'
        outputs[folder/'xr.html']=html
        # Original geometric concept: no cartridge graphics, characters or extracted scenery.
        svg='<svg xmlns="http://www.w3.org/2000/svg" width="1100" height="580" viewBox="0 0 1100 580"><rect width="1100" height="580" rx="24" fill="#101c2b"/><g fill="none" stroke="#395069" stroke-width="2"><path d="M50 420L200 480H1050L950 420ZM50 420V85H950V420"/><path d="M200 480V115L50 85"/></g><rect x="225" y="115" width="650" height="260" rx="10" fill="#20394a" stroke="#87c9bc" stroke-width="3"/><path d="M245 345H420V305H490V265H560V315H680V345H855" fill="none" stroke="#9fdbc5" stroke-width="12"/><rect x="505" y="242" width="164" height="130" rx="15" fill="#111923" stroke="#8b9daa" stroke-width="6"/><rect x="524" y="259" width="126" height="92" rx="12" fill="#34546a"/><path d="M524 315H560V315H650" fill="none" stroke="#9fdbc5" stroke-width="8"/><path d="M560 375L549 399H625L614 375" fill="#6d7c88"/><g fill="#20394a" stroke="#739cae" stroke-width="2"><rect x="805" y="400" width="230" height="136" rx="20"/>'
        for x in [830,895,960]:
            for y in [431,476]:svg+=f'<rect x="{x}" y="{y}" width="50" height="30" rx="8"/>'
        svg+='</g><path d="M758 546L889 455" stroke="#b9dded" stroke-width="3" stroke-dasharray="8 7"/><g font-family="sans-serif" fill="#e2eff4" text-anchor="middle">'
        for x,y,key in [(550,70,'questmural'),(590,230,'questcrtroom'),(920,389,'questremote'),(410,518,'questflooratlas')]:
            svg+=f'<text x="{x}" y="{y}" font-size="18">{h(tr(key))}</text>'
        svg+='</g></svg>\n';outputs[folder/'xr.svg']=svg
    outputs[ROOT/'docs/QUEST.md']='# Standalone Quest AR/VR preview\n\n[English illustrated guide](en/XR.md) · [Offline HTML](en/xr.html) · [Feature parity](PARITY.md)\n\n'+TEXT['status'][0]+'\n\n'+TEXT['install'][0]+'\n\n'+TEXT['placement'][0]+'\n\n'+TEXT['room'][0]+'\n\nAll ten language guides are linked from the English guide. No headset installation or runtime test is performed by the build workflow.\n'
    for path,text in outputs.items():
        if args.check:assert path.read_text(encoding='utf8')==text,f'Regenerate {path}'
        else:path.write_text(text,encoding='utf8',newline='\n')
    print('Ten localized XR guides and original spatial diagrams verified')

if __name__=='__main__':main()
