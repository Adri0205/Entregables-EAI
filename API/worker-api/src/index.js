import { DurableObject } from 'cloudflare:workers';
const cors={'Access-Control-Allow-Origin':'*','Access-Control-Allow-Methods':'GET, POST, OPTIONS','Access-Control-Allow-Headers':'Content-Type'};
const json=(value,status=200)=>new Response(JSON.stringify(value),{status,headers:{...cors,'Content-Type':'application/json'}});
const auto=d=>d>=100&&d<=200?'right':d>200&&d<=300?'left':'stop';
export class ControlState extends DurableObject {
 constructor(ctx,env){super(ctx,env);this.state={distance:45,mode:'manual',motor:'stop'};}
 async fetch(request){const url=new URL(request.url);if(request.method==='GET'&&url.pathname==='/api/status')return json(this.state);if(request.method!=='POST')return json({error:'Ruta no encontrada'},404);let body;try{body=await request.json()}catch{return json({error:'JSON inválido'},400)}
 if(url.pathname==='/api/distance'){if(typeof body.distance!=='number'||!Number.isFinite(body.distance)||body.distance<0||body.distance>400)return json({error:'Distancia debe estar entre 0 y 400'},400);this.state.distance=body.distance;}
 else if(url.pathname==='/api/mode'){if(!['manual','auto'].includes(body.mode))return json({error:'Modo inválido'},400);this.state.mode=body.mode;}
 else if(url.pathname==='/api/motor'){if(this.state.mode!=='manual')return json({error:'Motor bloqueado en modo automático'},409);if(!['left','right','stop'].includes(body.direction))return json({error:'Dirección inválida'},400);this.state.motor=body.direction;}
 else return json({error:'Ruta no encontrada'},404);
 if(this.state.mode==='auto')this.state.motor=auto(this.state.distance);return json(this.state);
 }
}
export default {async fetch(request,env){if(request.method==='OPTIONS')return new Response(null,{headers:cors});const id=env.CONTROL.idFromName('principal');return env.CONTROL.get(id).fetch(request)}};
