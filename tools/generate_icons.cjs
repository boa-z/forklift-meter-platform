/* Optional regeneration: npm install --no-save sharp; node tools/generate_icons.cjs */
const fs=require('fs'); const path=require('path'); const crypto=require('crypto');
const sharp=require(process.env.METER_SHARP_MODULE || 'sharp');
const root=path.resolve(__dirname,'..');
(async()=>{
  const manifest=JSON.parse(fs.readFileSync(path.join(root,'assets/manifest.json'),'utf8'));
  let h='#ifndef DEMO_ICONS_H\n#define DEMO_ICONS_H\n#include <lvgl.h>\n';
  let c='/* Generated from pinned MIT Tabler SVGs. See assets/manifest.json. */\n#include "generated/demo_icons.h"\n';
  for(const asset of manifest.icons){
    const source=fs.readFileSync(path.join(root,asset.file));
    if(crypto.createHash('sha256').update(source).digest('hex')!==asset.sha256) throw new Error('Source hash mismatch');
    const svg=source.toString('utf8').replace(/currentColor/g,'#b2dbd6');
    const {data,info}=await sharp(Buffer.from(svg)).resize(24,24).ensureAlpha().raw().toBuffer({resolveWithObject:true});
    if(info.channels!==4) throw new Error('Expected RGBA');
    const bgra=Buffer.alloc(data.length);
    for(let i=0;i<data.length;i+=4){bgra[i]=data[i+2];bgra[i+1]=data[i+1];bgra[i+2]=data[i];bgra[i+3]=data[i+3];}
    const name='demo_icon_'+asset.name.replaceAll('-','_');
    h+='extern const lv_image_dsc_t '+name+';\n';
    c+='static const uint8_t '+name+'_pixels[] = {\n';
    for(let i=0;i<bgra.length;i+=24)c+='    '+Array.from(bgra.subarray(i,i+24)).join(',')+',\n';
    c+='};\nconst lv_image_dsc_t '+name+' = { .header = { .magic=LV_IMAGE_HEADER_MAGIC, .cf=LV_COLOR_FORMAT_ARGB8888, .w=24, .h=24, .stride=96 }, .data_size=sizeof('+name+'_pixels), .data='+name+'_pixels };\n';
  }
  fs.writeFileSync(path.join(root,'generated/demo_icons.h'),h+'#endif\n'); fs.writeFileSync(path.join(root,'generated/demo_icons.c'),c);
  console.log('Generated five licensed icons using sharp '+require(path.join(path.dirname(require.resolve(process.env.METER_SHARP_MODULE || 'sharp')),'../package.json')).version);
})().catch(e=>{console.error(e);process.exit(1)});
