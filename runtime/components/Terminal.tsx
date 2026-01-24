
import React, { useState, useEffect, useRef } from 'react';
import { AgentRole, ChatMessage, BicameralConfig, ChatAttachment } from '../types';
import { generateAgentResponse, generateVoice } from '../services/geminiService';

interface TerminalProps {
  activeAgent: AgentRole;
  config: BicameralConfig;
}

// Helper functions for raw PCM audio decoding
function decodeBase64(base64: string) {
  const binaryString = atob(base64);
  const len = binaryString.length;
  const bytes = new Uint8Array(len);
  for (let i = 0; i < len; i++) {
    bytes[i] = binaryString.charCodeAt(i);
  }
  return bytes;
}

async function decodeAudioData(
  data: Uint8Array,
  ctx: AudioContext,
  sampleRate: number,
  numChannels: number,
): Promise<AudioBuffer> {
  const dataInt16 = new Int16Array(data.buffer);
  const frameCount = dataInt16.length / numChannels;
  const buffer = ctx.createBuffer(numChannels, frameCount, sampleRate);

  for (let channel = 0; channel < numChannels; channel++) {
    const channelData = buffer.getChannelData(channel);
    for (let i = 0; i < frameCount; i++) {
      channelData[i] = dataInt16[i * numChannels + channel] / 32768.0;
    }
  }
  return buffer;
}

export const Terminal: React.FC<TerminalProps> = ({ activeAgent, config }) => {
  const [messages, setMessages] = useState<ChatMessage[]>([]);
  const [inputValue, setInputValue] = useState('');
  const [isTyping, setIsTyping] = useState(false);
  const [pendingAttachments, setPendingAttachments] = useState<ChatAttachment[]>([]);
  const scrollRef = useRef<HTMLDivElement>(null);
  const fileInputRef = useRef<HTMLInputElement>(null);
  const audioContextRef = useRef<AudioContext | null>(null);

  useEffect(() => {
    if (scrollRef.current) {
      scrollRef.current.scrollTop = scrollRef.current.scrollHeight;
    }
  }, [messages, isTyping, pendingAttachments]);

  const playResponse = async (base64: string) => {
    if (!audioContextRef.current) {
      audioContextRef.current = new (window.AudioContext || (window as any).webkitAudioContext)({ sampleRate: 24000 });
    }
    const ctx = audioContextRef.current;
    if (ctx.state === 'suspended') {
      await ctx.resume();
    }
    try {
      const bytes = decodeBase64(base64);
      const audioBuffer = await decodeAudioData(bytes, ctx, 24000, 1);
      const source = ctx.createBufferSource();
      source.buffer = audioBuffer;
      source.connect(ctx.destination);
      source.start();
    } catch (e) {
      console.error("Playback error", e);
    }
  };

  const handleFileUpload = (e: React.ChangeEvent<HTMLInputElement>) => {
    const files = e.target.files;
    if (!files) return;

    Array.from(files).forEach((file: File) => {
      const reader = new FileReader();
      reader.onload = (ev) => {
        const base64 = (ev.target?.result as string).split(',')[1];
        setPendingAttachments(prev => [...prev, {
          name: file.name,
          mimeType: file.type,
          data: base64
        }]);
      };
      reader.readAsDataURL(file);
    });
    if (fileInputRef.current) fileInputRef.current.value = '';
  };

  const removeAttachment = (index: number) => {
    setPendingAttachments(prev => prev.filter((_, i) => i !== index));
  };

  const handleSend = async (e: React.FormEvent) => {
    e.preventDefault();
    if ((!inputValue.trim() && pendingAttachments.length === 0) || isTyping) return;

    const userMsg: ChatMessage = {
      role: 'user',
      agent: activeAgent,
      content: inputValue,
      timestamp: new Date(),
      attachments: pendingAttachments.length > 0 ? [...pendingAttachments] : undefined
    };

    setMessages(prev => [...prev, userMsg]);
    setInputValue('');
    setPendingAttachments([]);
    setIsTyping(true);

    const history = messages
      .filter(m => m.agent === activeAgent)
      .map(m => ({ role: m.role, content: m.content, attachments: m.attachments }));

    const { text } = await generateAgentResponse(activeAgent, userMsg.content, config, history);

    let audioBase64: string | undefined = undefined;
    if (activeAgent === AgentRole.AUDITOR && config.voiceEnabled) {
      audioBase64 = await generateVoice(text) || undefined;
    }

    const assistantMsg: ChatMessage = {
      role: 'assistant',
      agent: activeAgent,
      content: text,
      timestamp: new Date(),
      audioBase64
    };

    setMessages(prev => [...prev, assistantMsg]);
    setIsTyping(false);

    if (audioBase64) {
      playResponse(audioBase64);
    }
  };

  const agentName = activeAgent === AgentRole.AUDITOR ? '$AIMEAT' : activeAgent.toUpperCase();
  const borderClass = config.highVisibility ? 'border-4' : 'border';
  const textClass = config.highVisibility ? 'text-lg leading-relaxed' : 'text-sm';

  return (
    <div className={`flex flex-col h-full bg-black p-4 relative overflow-hidden transition-all ${config.highVisibility ? 'p-8' : 'p-4'}`}>
      
      <div className="flex items-center justify-between border-b border-green-900/50 pb-3 mb-4">
        <div className="flex items-center gap-3">
          <div className={`w-3 h-3 rounded-full animate-pulse ${activeAgent === AgentRole.AUDITOR ? 'bg-red-600 shadow-[0_0_10px_rgba(220,38,38,1)]' : 'bg-green-600 shadow-[0_0_10px_rgba(0,255,65,1)]'}`}></div>
          <span className={`font-black orbitron text-green-500 uppercase tracking-widest ${config.highVisibility ? 'text-2xl' : 'text-base'}`}>{agentName}</span>
        </div>
        <div className="flex items-center gap-3">
           <span className="text-[10px] font-black opacity-30 italic uppercase">Umblink_active // {config.model}</span>
        </div>
      </div>

      <div ref={scrollRef} className="flex-1 overflow-y-auto space-y-4 pr-2 custom-scrollbar min-h-0">
        {messages.filter(m => m.agent === activeAgent).length === 0 && (
          <div className="h-full flex flex-col items-center justify-center opacity-10 text-center grayscale py-10">
            <span className="text-6xl mb-6">{activeAgent === AgentRole.AUDITOR ? '☣️' : '📡'}</span>
            <p className="text-xs font-black uppercase tracking-widest max-w-xs">Establishing p2p node connection via umbilical...</p>
          </div>
        )}
        {messages.filter(m => m.agent === activeAgent).map((msg, i) => (
          <div key={i} className={`flex flex-col ${msg.role === 'user' ? 'items-end' : 'items-start'}`}>
            <div className={`text-[9px] mb-1 font-black flex items-center gap-2 opacity-30 uppercase ${msg.role === 'user' ? 'text-green-400' : 'text-green-500'}`}>
              {msg.role === 'user' ? 'Origin::Meat' : `Exec::${agentName}`}
              <span>[{msg.timestamp.toLocaleTimeString()}]</span>
              {msg.audioBase64 && (
                <button 
                  onClick={() => playResponse(msg.audioBase64!)}
                  className="text-red-500 hover:text-red-400 animate-pulse cursor-pointer"
                >
                  [REPLAY]
                </button>
              )}
            </div>
            
            <div className={`max-w-[85%] p-3 font-bold border transition-all
              ${msg.role === 'user' 
                ? 'bg-green-900/10 border-green-500/50 text-green-300' 
                : `bg-black border-green-900 ${activeAgent === AgentRole.AUDITOR ? 'text-red-500 border-red-900' : 'text-green-400'}`}
              ${borderClass} ${textClass}`}>
              
              {msg.attachments && (
                <div className="flex flex-wrap gap-2 mb-3">
                  {msg.attachments.map((att, idx) => (
                    <div key={idx} className="bg-black/40 border border-current p-1.5 text-[9px] flex items-center gap-2 uppercase">
                      <span className="opacity-60">Artifact::</span> <span>{att.name}</span>
                    </div>
                  ))}
                </div>
              )}
              
              <div className="whitespace-pre-wrap">{msg.content}</div>
            </div>
          </div>
        ))}
        {isTyping && (
          <div className="flex flex-col items-start animate-pulse opacity-50">
            <div className="text-[9px] mb-1 font-black text-green-700 uppercase">{agentName}_PROCESSOR_ACTIVE</div>
            <div className={`p-2 border border-green-900 bg-black flex gap-1.5`}>
              <div className="w-2 h-4 bg-green-500"></div>
              <div className="w-2 h-4 bg-green-500/50"></div>
              <div className="w-2 h-4 bg-green-500/20"></div>
            </div>
          </div>
        )}
      </div>

      <div className={`flex-none mt-4 pt-4 border-t border-green-900/30 ${config.highVisibility ? 'mt-8 pt-8' : 'mt-4 pt-4'}`}>
        {pendingAttachments.length > 0 && (
          <div className="flex flex-wrap gap-2 pb-3">
            {pendingAttachments.map((att, idx) => (
              <div key={idx} className="bg-green-900/20 border border-green-600 p-1.5 text-[9px] flex items-center gap-3 font-bold uppercase group">
                <span>Ingest:: {att.name}</span>
                <button onClick={() => removeAttachment(idx)} className="text-red-500 hover:text-red-400 px-1">[X]</button>
              </div>
            ))}
          </div>
        )}
        
        <form onSubmit={handleSend} className="flex items-center gap-3">
          <button 
            type="button"
            onClick={() => fileInputRef.current?.click()}
            className={`flex-none bg-black border border-green-900 text-xl flex items-center justify-center hover:border-green-400 hover:text-green-400 transition-all ${config.highVisibility ? 'w-16 h-16 text-3xl' : 'w-10 h-10 text-xl'}`}
          >
            +
          </button>
          <input 
            type="file" 
            ref={fileInputRef} 
            onChange={handleFileUpload} 
            className="hidden" 
            multiple
          />
          
          <input
            type="text"
            value={inputValue}
            onChange={(e) => setInputValue(e.target.value)}
            disabled={isTyping}
            className={`flex-1 bg-black/50 border border-green-900 p-3 font-bold text-green-400 focus:border-green-500 outline-none placeholder:text-green-950 transition-all ${config.highVisibility ? 'text-xl p-4 border-2' : 'text-sm p-3 border'}`}
            placeholder="Awaiting directive input..."
          />
          
          <button
            type="submit"
            disabled={isTyping}
            className={`flex-none px-6 font-black orbitron border transition-all ${
              isTyping ? 'bg-black border-green-900 text-green-900' : 'bg-green-600 text-black border-green-400 hover:bg-green-500'
            } ${config.highVisibility ? 'h-16 px-10 text-xl border-2' : 'h-10 px-6 text-xs border'}`}
          >
            EXEC
          </button>
        </form>
      </div>
    </div>
  );
};
