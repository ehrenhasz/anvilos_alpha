
import React from 'react';
import { BicameralConfig, GeminiModel } from '../types';

interface SystemConfigProps {
  config: BicameralConfig;
  setConfig: React.Dispatch<React.SetStateAction<BicameralConfig>>;
}

const MODELS: { label: string; value: GeminiModel }[] = [
  { label: 'GEMINI_2.0_FLASH (EXP)', value: 'gemini-2.0-flash-exp' },
  { label: 'GEMINI_1.5_FLASH (FAST)', value: 'gemini-1.5-flash' },
  { label: 'GEMINI_2.0_FLASH (STD)', value: 'gemini-2.0-flash' },
  { label: 'GEMINI_FLASH_LITE (LITE)', value: 'gemini-flash-lite-latest' },
];

export const SystemConfig: React.FC<SystemConfigProps> = ({ config, setConfig }) => {
  const handleKeySelection = async () => {
    try {
      // GUIDELINE: window.aistudio is pre-configured
      await (window as any).aistudio.openSelectKey();
      setConfig(prev => ({ ...prev, apiTier: 'STUDIO_PAID' }));
    } catch (e) {
      console.error("Key selection failed:", e);
    }
  };

  const setTier = (tier: BicameralConfig['apiTier']) => {
    setConfig(prev => ({ ...prev, apiTier: tier }));
  };

  return (
    <div className="h-full flex flex-col gap-6 p-4 animate-in fade-in zoom-in-95 duration-300 overflow-y-auto custom-scrollbar bg-black">
      <div className="flex items-center justify-between border-b border-green-900 pb-2">
        <h2 className="text-lg font-black orbitron tracking-tighter text-green-500 uppercase">System_Parameters</h2>
        <span className="text-[8px] bg-red-600 text-white px-2 py-0.5 animate-pulse font-black">ROOT_ACCESS_LEVEL_5</span>
      </div>

      <div className="grid grid-cols-1 md:grid-cols-2 gap-8">
        {/* API ACCESS */}
        <div className="space-y-4 bg-green-950/5 p-4 border border-green-900/30 shadow-[inset_0_0_10px_rgba(0,0,0,1)]">
          <label className="text-[10px] font-black text-green-900 block mb-2 border-l-2 border-green-600 pl-2 uppercase">Auth_Priorities</label>
          <div className="space-y-2">
            {[
              { id: 'FREE', label: 'T-0: FREE_STUDIO', color: 'text-green-800' },
              { id: 'STUDIO_PAID', label: 'T-1: PAID_CREDENTIALS', color: 'text-green-500' },
              { id: 'VERTEX', label: 'T-2: VERTEX_PROVISION', color: 'text-cyan-500' },
            ].map(tier => (
              <button 
                key={tier.id}
                onClick={() => setTier(tier.id as any)}
                className={`w-full p-3 text-left border flex justify-between items-center transition-all ${
                  config.apiTier === tier.id 
                    ? 'bg-green-600 text-black border-green-400 font-black' 
                    : 'bg-black border-green-900/50 text-green-900 opacity-60 hover:opacity-100 hover:border-green-700'
                }`}
              >
                <span className="text-[10px] font-black orbitron">{tier.label}</span>
                {config.apiTier === tier.id && <span className="text-[8px] font-black animate-pulse">● ACTIVE</span>}
              </button>
            ))}
          </div>
          <button 
            onClick={handleKeySelection}
            className="w-full mt-4 py-2 bg-black border border-green-500 text-green-500 text-[9px] font-black orbitron hover:bg-green-500 hover:text-black transition-all shadow-[2px_2px_0px_0px_rgba(0,255,65,0.1)]"
          >
            ROTATE_AUTH_CREDENTIALS
          </button>
        </div>

        {/* INFERENCE ENGINE */}
        <div className="space-y-4">
          <div className="space-y-2">
            <label className="text-[10px] font-black text-green-900 block mb-2 border-l-2 border-green-600 pl-2 uppercase">Model_Target</label>
            <select 
              value={config.model}
              onChange={(e) => setConfig({ ...config, model: e.target.value as GeminiModel })}
              className="w-full bg-black border border-green-900 p-3 text-[10px] font-black orbitron text-green-500 focus:border-green-400 outline-none appearance-none cursor-pointer hover:border-green-700 transition-colors"
            >
              {MODELS.map((m) => (
                <option key={m.value} value={m.value} className="bg-black text-green-500 p-2">{m.label}</option>
              ))}
            </select>
          </div>

          <div className="space-y-6 pt-4">
            <label className="text-[10px] font-black text-green-900 block border-l-2 border-green-600 pl-2 uppercase">Inference_Tuning</label>
            
            <div className="space-y-2">
              <div className="flex justify-between text-[9px] font-black uppercase text-green-900">
                <span>Entropy_Coefficient</span>
                <span className="text-green-500">[{config.temperature}]</span>
              </div>
              <input 
                type="range" min="0" max="2" step="0.1"
                value={config.temperature}
                onChange={(e) => setConfig({ ...config, temperature: parseFloat(e.target.value) })}
                className="w-full accent-green-500 h-1 bg-green-950 appearance-none cursor-pointer"
              />
            </div>

            <div className="space-y-2">
              <div className="flex justify-between text-[9px] font-black uppercase text-green-900">
                <span>Nucleus_P_Factor</span>
                <span className="text-green-500">[{config.topP}]</span>
              </div>
              <input 
                type="range" min="0" max="1" step="0.05"
                value={config.topP}
                onChange={(e) => setConfig({ ...config, topP: parseFloat(e.target.value) })}
                className="w-full accent-green-500 h-1 bg-green-950 appearance-none cursor-pointer"
              />
            </div>
          </div>
        </div>
      </div>

      <div className="mt-auto bg-green-950/10 border border-green-900 p-3 text-[9px] font-mono leading-relaxed relative shadow-[inset_0_0_10px_rgba(0,0,0,1)]">
        <div className="absolute top-1 right-2 text-[7px] text-green-900 italic uppercase font-black">Hash_v99.0_Checksum</div>
        <span className="text-green-400 font-black">[!] Status:</span> Nominally Sovereign<br/>
        <span className="text-green-400 font-black">[!] Mode:</span> Aggressive Lobotomy via VirtualMeat<br/>
        <span className="text-green-400 font-black">[!] Deployment:</span> Black Ops v99.0
      </div>
    </div>
  );
};
