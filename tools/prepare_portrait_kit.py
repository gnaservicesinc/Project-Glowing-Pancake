#!/usr/bin/env python3
"""Reproduce the reviewed vector layer kit. No downloads or pixel images."""
import argparse
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
TARGET = ROOT / 'assets/portraits/kit-v1.json'

def path(d, fill='none', stroke='@ink', width=2):
    return f'<path d="{d}" fill="{fill}" stroke="{stroke}" stroke-width="{width}" stroke-linecap="round" stroke-linejoin="round"/>'

def ellipse(cx,cy,rx,ry,fill,stroke='none',width=2):
    return f'<ellipse cx="{cx}" cy="{cy}" rx="{rx}" ry="{ry}" fill="{fill}" stroke="{stroke}" stroke-width="{width}"/>'

def prepare():
    # Original frontal silhouettes share fixed eye/nose anchors. Profile-source
    # paths cannot be combined with a frontal feature rig.
    heads=[
        'M77 92 Q76 52 120 50 Q164 52 163 92 L160 127 Q156 157 120 171 Q84 157 80 127Z',
        'M78 91 Q76 51 120 50 Q164 51 162 91 L159 137 Q150 163 120 168 Q90 163 81 137Z',
        'M76 94 Q73 51 120 50 Q167 51 164 94 L161 131 Q151 166 120 175 Q89 166 79 131Z',
        'M78 89 Q78 52 120 51 Q162 52 162 89 L164 122 Q157 158 120 168 Q83 158 76 122Z',
        'M80 88 Q78 50 120 49 Q162 50 160 88 L158 128 Q152 159 120 177 Q88 159 82 128Z',
        'M75 95 Q75 52 120 50 Q165 52 165 95 L162 132 Q153 156 137 166 L103 166 Q87 156 78 132Z']
    hair_back=['',
        path('M70 93 Q61 28 120 28 Q179 28 170 93 L160 112 L158 72 Q119 46 82 72 L80 112Z','@hair'),
        path('M65 92 Q58 25 120 26 Q182 25 175 92 L184 190 Q157 204 143 178 L98 178 Q82 203 56 190Z','@hair'),
        path('M66 97 Q57 26 120 26 Q183 26 174 97 L178 165 L62 165Z','@hair'),
        path('M68 103 Q55 88 63 70 Q54 47 78 41 Q83 22 105 29 Q123 16 141 29 Q166 23 173 44 Q190 52 179 74 Q187 97 169 109 L154 73 L84 73Z','@hair'),
        path('M67 92 Q60 25 120 26 Q179 25 173 92 L179 187 L144 184 L149 75 L86 75 L96 184 L61 187Z','@hair'),
        path('M73 92 Q59 39 98 28 Q140 15 166 53 Q177 69 168 96 L157 79 L83 79Z','@hair'),
        path('M70 95 Q60 23 120 27 Q181 23 170 95 L160 130 L154 77 L85 77 L80 130Z','@hair'),
        path('M72 93 Q65 30 120 28 Q175 30 168 93 L159 117 L155 73 L85 73 L81 117Z','@hair')]
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
    for ry,iris in [(4,3.2),(3,2.8),(4.5,3.5),(3.5,3)]:
        eyes.append(''.join(ellipse(x,103,9,ry,'@paper','@ink',1.5)+ellipse(x+1,103,iris,ry-1,'@eye')+ellipse(x+1,103,1.5,2.5,'@ink')+ellipse(x+2,101,1,1,'@paper') for x in (98,142)))
    brows=[path('M87 89 Q98 84 108 89 M132 89 Q143 84 154 89','none','@hair',4),path('M87 88 L108 86 M132 86 L154 88','none','@hair',3),path('M87 91 Q98 82 108 89 M132 89 Q143 82 154 91','none','@hair',5),path('M88 88 L107 89 M133 89 L152 88','none','@hair',2)]
    facial=['',path('M106 136 Q114 130 121 135 Q128 130 135 136 L129 141 L120 138 L111 141Z','@hair','none'),path('M93 139 Q98 165 120 170 Q143 166 149 139 L141 151 Q120 164 101 151Z','@hair','none'),path('M114 155 Q120 158 127 155 L125 167 L117 167Z','@hair','none')]
    glasses=['',path('M83 97 H112 V111 Q99 118 86 111Z M128 97 H157 L154 111 Q141 118 128 111Z M112 101 Q120 97 128 101 M78 100 L83 100 M157 100 L162 100','none','@ink',2.5),ellipse(99,104,16,13,'none','@ink',2)+ellipse(141,104,16,13,'none','@ink',2)+path('M115 101 Q120 98 125 101 M78 101 L83 101 M157 101 L162 101'),path('M83 97 Q98 94 113 98 L110 112 Q96 118 85 111Z M127 98 Q142 94 157 97 L155 111 Q142 118 130 112Z M113 101 H127','none','@ink',3)]
    return {'kit':1,'viewBox':[0,0,240,240], 'source':'Original project vector art, GPL-3.0; frontal portrait rig, 2026-11-10.',
        'head':[path(d,'@skin','@ink',1.4) for d in heads], 'hairBack':hair_back,'hairFront':hair_front,
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
