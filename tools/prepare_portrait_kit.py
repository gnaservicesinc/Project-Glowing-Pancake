#!/usr/bin/env python3
"""Reproduce the reviewed vector layer kit. No downloads or pixel images."""
import argparse
import hashlib
import json
import xml.etree.ElementTree as ET
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / 'assets/portraits/sources/bitclone-basic-face-kit-2.svg'
TARGET = ROOT / 'assets/portraits/kit-v1.json'
# Source bounds measured with QtSvg, including the retained source layer transform.
BOUNDS = {'path9262': [2370.48388532257, 156.70652052379592, 137.4300026266028, 122.33476696761863], 'path4965-5-9': [2661.837110490629, 156.70628999395194, 119.1970500088537, 126.93366736999815], 'path9192': [2675.213992747904, 338.77883701766405, 120.87404258419701, 132.11802537928452], 'path8469': [2939.804884239078, 156.6447311912525, 127.73082240038639, 120.28810512721975], 'path8936': [3158.4328444774656, 157.9859233524512, 110.53444473371201, 116.22254019707543], 'path8849': [3416.6110436588388, 141.86012019181726, 116.74938163020852, 132.93446416180814], 'path10061': [3774.263498743974, 241.31757645146354, 304.43714359225487, 354.1160571166365], 'path9561': [4240.328925800279, 379.56382319017575, 175.1691073703514, 157.93838567586863], 'path8741': [4585.668175004308, 297.9656074108789, 170.27381993737436, 245.65138489129095], 'path9254': [4087.0740659471085, 719.872388114265, 203.09926313096003, 170.3573047227328], 'path8907': [4055.694805058507, 1084.4957842342883, 162.49751528028173, 177.66611380741597], 'path8980': [4380.685288905079, 1098.9858198425281, 187.87535488132744, 188.1310249237904], 'path9071': [4657.9882065790625, 1077.546541820051, 169.0166573653023, 165.81097485903956], 'path9755': [4884.397375690545, 309.2428411773217, 206.12203665033303, 203.01790168802472], 'path4365-0': [2396.9495192016366, 913.6828510742889, 126.30558768699848, 128.60163774876855], 'path4365-0-4': [2564.6050344784417, 913.3573361323397, 124.6816378355652, 127.97484260008787], 'path10665': [2235.989563116651, 1375.7713747466914, 134.8982837424919, 141.40552894355278], 'path10665-8': [2419.8830208747313, 1378.6124405838236, 140.70563439636317, 142.61446310641986], 'path10665-8-9': [2598.925173831958, 1381.474438415964, 139.35473033642847, 142.6144653550764], 'path8259-3': [2596.3923799514996, 1845.8438740915371, 141.89836203962113, 145.72766161007894]}
SOURCE_SHA256 = 'f630c46704983b5466aceb90da2e06bb0d3015c2f35f084c45b3d5c5eee37a6b'

def path(d, fill='none', stroke='@ink', width=2):
    return f'<path d="{d}" fill="{fill}" stroke="{stroke}" stroke-width="{width}" stroke-linecap="round" stroke-linejoin="round"/>'

def ellipse(cx,cy,rx,ry,fill,stroke='none',width=2):
    return f'<ellipse cx="{cx}" cy="{cy}" rx="{rx}" ry="{ry}" fill="{fill}" stroke="{stroke}" stroke-width="{width}"/>'

def prepare():
    if hashlib.sha256(SOURCE.read_bytes()).hexdigest()!=SOURCE_SHA256:
        raise ValueError('Portrait source changed; review the layer IDs and geometry before regenerating.')
    root=ET.parse(SOURCE).getroot();layer=root.find('{http://www.w3.org/2000/svg}g');nodes={n.get('id'):n for n in layer}
    def extract(id,box,color):
        x,y,w,h=BOUNDS[id];tx,ty,tw,th=box
        # Use only the reviewed path geometry, dropping editor metadata/effects.
        node=nodes[id];shape=path(node.get('d'),color,'@ink',2.0/(tw/w))
        if node.get('transform'): shape=f'<g transform="{node.get("transform")}">{shape}</g>'
        return f'<g transform="translate({tx-x*tw/w:.6f} {ty-y*th/h:.6f}) scale({tw/w:.8f} {th/h:.8f})"><g transform="{layer.get("transform")}">{shape}</g></g>'
    heads=['path4365-0', 'path4365-0-4', 'path10665', 'path10665-8', 'path10665-8-9', 'path8259-3']
    backs=['path10061','path9561','path8741','path9254','path8907','path8980','path9071','path9755']
    hair_back=['']+[extract(id,(56,35,128,164),'@hair') for id in backs]
    hair_front=['',
        path('M75 75 Q65 28 112 30 Q164 25 168 74 Q141 64 128 44 Q114 67 75 75Z','@hair'),
        path('M72 75 Q65 31 112 31 L155 30 Q175 42 168 83 L148 61 L140 42 Q115 66 72 75Z','@hair'),
        path('M72 81 Q66 30 120 30 Q174 30 169 83 L150 78 L145 53 L135 81 L113 79 L110 55 L98 80Z','@hair'),
        path('M70 75 Q64 26 120 29 Q174 25 170 77 Q159 61 150 68 Q142 47 133 58 Q120 44 111 58 Q91 50 87 72Z','@hair'),
        path('M74 79 Q66 32 115 28 Q168 25 170 80 Q154 57 142 56 Q112 85 74 79Z','@hair'),
        path('M74 74 Q67 32 120 30 Q172 30 166 76 Q143 62 121 45 Q100 64 74 74Z','@hair'),
        path('M75 73 Q69 33 121 31 Q169 29 166 74 L144 55 L126 73 L114 49 L96 72Z','@hair'),
        path('M74 70 Q72 30 120 30 Q166 30 167 70 Q140 48 120 47 Q96 47 74 70Z','@hair')]
    noses=[path('M121 106 L116 125 Q121 129 127 124'),path('M119 109 Q109 126 120 128 L129 124'),path('M120 108 L123 126 M116 128 Q122 131 129 127'),path('M121 109 L115 122 Q111 128 119 129 L128 127'),path('M120 111 Q116 119 120 124 M113 126 Q120 131 128 126')]
    ears=[]
    for rx,ry in [(9,16),(7,12),(11,14)]:
        ears.append(ellipse(73,110,rx,ry,'@skin','@ink')+ellipse(167,110,rx,ry,'@skin','@ink')+path('M73 105 Q80 112 74 118 M167 105 Q160 112 166 118','none','@shade',2))
    mouths=[path('M105 143 Q120 153 136 141','none','@lip',2.5),path('M105 141 Q120 145 136 140 Q132 157 121 157 Q111 155 105 141Z','@lip','@ink',1.5)+path('M108 142 Q121 145 133 141 L130 147 L111 148Z','@paper','none'),path('M108 146 Q120 143 132 145','none','@lip',3),path('M107 143 Q120 149 134 142 M116 152 L126 152','none','@lip',2),path('M109 143 Q119 136 131 143 Q120 153 109 143Z','@lip','none')]
    chins=['',path('M114 160 Q120 163 126 160','none','@shade',1.5),path('M118 159 L121 161 L124 159','none','@shade',1.5)]
    necks=[path('M103 153 L102 184 Q120 200 138 184 L136 152Z','@skin'),path('M107 151 L106 185 Q121 195 134 185 L132 151Z','@skin'),path('M99 152 L97 185 Q120 202 143 185 L141 151Z','@skin')]
    eyes=[]
    for ry,iris in [(6,4),(4.5,3.5),(7,4.5),(5,3.5)]:
        eyes.append(''.join(ellipse(x,103,11,ry,'@paper','@ink',1.5)+ellipse(x+1,103,iris,ry-1,'@eye')+ellipse(x+1,103,2,3,'@ink')+ellipse(x+2,101,1,1,'@paper') for x in (98,142)))
    brows=[path('M87 89 Q98 84 108 89 M132 89 Q143 84 154 89','none','@hair',4),path('M87 88 L108 86 M132 86 L154 88','none','@hair',3),path('M87 91 Q98 82 108 89 M132 89 Q143 82 154 91','none','@hair',5),path('M88 88 L107 89 M133 89 L152 88','none','@hair',2)]
    facial=['',path('M106 136 Q114 130 121 135 Q128 130 135 136 L129 141 L120 138 L111 141Z','@hair','none'),path('M93 139 Q98 165 120 170 Q143 166 149 139 L141 151 Q120 164 101 151Z','@hair','none'),path('M114 155 Q120 158 127 155 L125 167 L117 167Z','@hair','none')]
    glasses=['',path('M83 97 H112 V111 Q99 118 86 111Z M128 97 H157 L154 111 Q141 118 128 111Z M112 101 Q120 97 128 101 M78 100 L83 100 M157 100 L162 100','none','@ink',2.5),ellipse(99,104,16,13,'none','@ink',2)+ellipse(141,104,16,13,'none','@ink',2)+path('M115 101 Q120 98 125 101 M78 101 L83 101 M157 101 L162 101'),path('M83 97 Q98 94 113 98 L110 112 Q96 118 85 111Z M127 98 Q142 94 157 97 L155 111 Q142 118 130 112Z M113 101 H127','none','@ink',3)]
    return {'kit':1,'viewBox':[0,0,240,240], 'source':'Bitclone Basic Face Kit 2, VexStrips / future, Openclipart CC0; original complementary game layers.',
        'head':[extract(id,(75,53,90,116),'@skin') for id in heads], 'hairBack':hair_back,'hairFront':hair_front,
        'nose':noses,'ears':ears,'mouth':mouths,'chin':chins,'neck':necks,'eyes':eyes,'brows':brows,'facialHair':facial,'glasses':glasses,
        'skin':['#f3d3b3','#e4b08a','#cb936c','#b57952','#995f43','#7b4936','#5c392e','#dfbaa0'],
        'hairColor':['#273332','#46352e','#714b32','#a96c3d','#c3a371','#674a43','#a6a9a0','#d8d7c7'],
        'shirtColor':['#356a69','#a46a4c','#4f6684','#756487','#748259','#b18b48'],
        'background':['#c5d6c6','#dfcead','#b8cdcd','#d3c5bf','#bbc4d2','#c9cbb7']}

if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--check',action='store_true');a=p.parse_args()
    result=json.dumps(prepare(),ensure_ascii=False,indent=2)+'\n'
    if a.check:
        if TARGET.read_text()!=result:raise SystemExit('Portrait kit differs from its reviewed source')
        print('Portrait kit reproduced exactly')
    else:TARGET.write_text(result)
