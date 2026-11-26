"""Export the hand-drawn launcher icon for desktop and Android from one editable SVG."""
from io import BytesIO
from pathlib import Path
from xml.etree import ElementTree as ET

from PIL import Image
from resvg_py import svg_to_bytes

ROOT = Path(__file__).resolve().parents[1]
ANDROID = 'http://schemas.android.com/apk/res/android'


def write_icons():
    assets = ROOT / 'assets'
    source = assets / 'icon.svg'
    with Image.open(BytesIO(svg_to_bytes(svg_path=str(source), skip_system_fonts=True))) as image:
        image.save(assets / 'icon.png', optimize=True)
        image.save(assets / 'icon.ico', sizes=[(n, n) for n in (16, 24, 32, 48, 64, 128, 256)])
        image.save(assets / 'icon.icns')

    res = ROOT / 'android/app/src/main/res'
    paths = ET.parse(source).getroot().findall('{http://www.w3.org/2000/svg}path')
    ET.register_namespace('android', ANDROID)

    def attrs(**values):
        return {f'{{{ANDROID}}}{name}': value for name, value in values.items()}

    def write(resource, tree):
        path = res / resource
        path.parent.mkdir(parents=True, exist_ok=True)
        ET.indent(tree, space='    ')
        path.write_text(ET.tostring(tree, encoding='unicode') + '\n', encoding='utf-8', newline='\n')

    for adaptive in (False, True):
        vector = ET.Element('vector', attrs(width='108dp', height='108dp', viewportWidth='108',
                                            viewportHeight='108'))
        # The subject fits the 66dp safe zone of Android's maskable foreground.
        target = ET.SubElement(vector, 'group', attrs(pivotX='54', pivotY='54', scaleX='.76',
                                                       scaleY='.76')) if adaptive else vector
        for path in paths:
            if adaptive and path.attrib['id'] in {'badge', 'shadow'}:
                continue
            ET.SubElement(target, 'path', attrs(fillColor=path.attrib['fill'],
                                                pathData=path.attrib['d']))
        name = 'mcu_mushroom' if adaptive else 'mcu_icon'
        write(f'drawable/{name}.xml', vector)

    icon = ET.Element('adaptive-icon')
    badge = next(path for path in paths if path.attrib['id'] == 'badge')
    colors = ET.Element('resources')
    ET.SubElement(colors, 'color', name='mcu_icon_badge').text = badge.attrib['fill']
    write('values/mcu_icon_colors.xml', colors)
    ET.SubElement(icon, 'background', attrs(drawable='@color/mcu_icon_badge'))
    ET.SubElement(icon, 'foreground', attrs(drawable='@drawable/mcu_mushroom'))
    write('drawable-v26/mcu_icon.xml', icon)


if __name__ == '__main__':
    write_icons()
