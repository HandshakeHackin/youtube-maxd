// "Notify me" reminder of an upcoming stream or premiere. The pinned parser
// keeps neither the toggle nor its state on the watch page, so the control is
// read from the raw player response: exactly one toggle button whose default
// endpoint adds and whose toggled endpoint removes the reminder. Anything else
// sends nothing. SPDX-License-Identifier: GPL-3.0-or-later
import {Intents,accepted,collect} from './intents.mjs';

const paths={set:'notification/add_upcoming_event_reminder',unset:'notification/remove_upcoming_event_reminder'};
const params=value=>typeof value==='string' && value.length>0 && value.length<=4096?value:'';

export function reminderControl(raw) {
  const found=[];
  for(const button of collect(raw?.playabilityStatus,'toggleButtonRenderer',16)) {
    const set=params(button.defaultServiceEndpoint?.addUpcomingEventReminderEndpoint?.params);
    const unset=params(button.toggledServiceEndpoint?.removeUpcomingEventReminderEndpoint?.params);
    if(set && unset && typeof button.isToggled==='boolean' && button.isDisabled!==true)
      found.push({set,unset,reminder_set:button.isToggled});
  }
  return found.length===1?found[0]:null;
}
export class StreamReminders {
  constructor(ErrorType,clock=Date.now) {this.ErrorType=ErrorType;this.intents=new Intents(ErrorType,clock);}
  async read(client,id) {
    const response=await client.actions.execute('/player',{videoId:id});
    const raw=response?.data,details=raw?.videoDetails;
    if(!accepted(response) || details?.videoId!==id)
      throw new this.ErrorType('upstream_format','YouTube returned reminder data for a different video.',502);
    return {upcoming:details.isUpcoming===true,control:details.isUpcoming===true?reminderControl(raw):null};
  }
  async run(owner,client,input) {
    const wanted=input.reminder;
    if(!['status','set','unset'].includes(wanted))
      throw new this.ErrorType('invalid_request','Select status, set or unset.');
    if(wanted==='status') {
      const {upcoming,control}=await this.read(client,input.id);
      return {api_version:1,kind:'stream_reminder',id:input.id,upcoming,available:!!control,reminder_set:!!control?.reminder_set};
    }
    if(!client.session?.logged_in)throw new this.ErrorType('account_required','Sign in to the selected account first.',401);
    return this.intents.once(owner,input.session,`${input.id}:${wanted}`,'reminder_unconfirmed',async release=>{
      let control;
      try {({control}=await this.read(client,input.id));} catch(error) {release();throw error;}
      if(!control) {
        release();
        throw new this.ErrorType('reminder_unavailable','YouTube did not offer a reminder for this video.',404);
      }
      const result={api_version:1,success:true,id:input.id,reminder_set:wanted==='set'};
      if(control.reminder_set===result.reminder_set)return result;
      if(!accepted(await client.actions.execute(paths[wanted],{params:control[wanted]})))
        throw new this.ErrorType('reminder_unconfirmed','YouTube did not confirm the reminder change.',502);
      return result;
    });
  }
}
