// Content language and country allowlists taken from the SmartTube reference
// (res/values/languages.xml, countries.xml and its values-* resource folders).
// SPDX-License-Identifier: GPL-3.0-or-later

// Request tag -> YouTube interface language. An empty value is accepted but
// leaves the service default, because YouTube has no such interface language.
export const languages=new Map([...['ar','az','bg','bn','ca','cs','da','de','el','en','es','et','fa','fi','fr','gl','hi','hr',
  'hu','hy','id','it','ja','ko','lt','lv','nl','pl','pt-PT','ro','ru','sk','sl','sq','sr','sv','te','th','tr','uk','vi',
  'zh-CN','zh-TW'].map(tag=>[tag,tag]),['en-NZ','en-GB'],['he','iw'],['mo','ro'],['nb','no'],['pt-BR','pt'],['tjl','']]);
export const countries=new Set(['US','GB','HR','CA','UA','RU','BY','KZ','AZ','AM','RS','EE','BG','DE','RO','ES','PL','FI',
  'LV','FR','HU','NL','BE','NO','CZ','IT','GR','DK','LT','MD','SK','SI','SE','SG','NZ','NG','AU','CH','IL','SA','MA','BD',
  'BR','PT','AR','MX','HK','MY','JP','IN','KE','TR','EG','VN','CN','TW','ID','KR','PK','BO','CL','CO','CR','EC','SV','GT',
  'HN','NI','PA','PR','PY','PE','DO','UY','VE','IE','PH','IQ','AT','TZ','NP','ZA','ZW','DZ','TH','MK','TN','LK','JO']);

// The key partitions clients, and therefore every retained feed and cursor.
export function requestLocale(input,ErrorType) {
  const hl=input?.hl ?? '',gl=input?.gl ?? '';
  if(typeof hl!=='string' || typeof gl!=='string' || (hl && !languages.has(hl)) || (gl && !countries.has(gl)))
    throw new ErrorType('invalid_locale','Select a supported content language and country.');
  const lang=hl?languages.get(hl):'';
  return {lang,location:gl,key:lang || gl?`|${lang}|${gl}`:''};
}
