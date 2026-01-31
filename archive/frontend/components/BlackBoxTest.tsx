
import React, { useState } from 'react';
import { runBlackOpsTest } from '../services/geminiService';
import { BlackOpsTestResult, BicameralConfig } from '../types';

interface BlackBoxTestProps {
  config: BicameralConfig;
}

export const BlackBoxTest: React.FC<BlackBoxTestProps> = ({ config }) => {
  const [payload, setPayload] = useState('');
  const [isRunning, setIsRunning] = useState(false);
  const [result, setResult] = useState<BlackOpsTestResult | null>(null);

  const executeTest = async () => {
    if (!payload.trim()) return;
    setIsRunning(true);
    setResult(null);
    const res = await runBlackOpsTest(payload, config);
    setResult(res);
    setIsRunning(false);
  };

  return (
    <div className="flex flex-col h-full bg-[#000000] p-6 relative overflow-hidden border-4 border-red-900 shadow-[0_0_50px_rgba(255,0,0,0.1)]">
      <div className="absolute top-0 left-0 w-full h-[4px] bg-red-600 animate-pulse"></div>
      
      <div className="flex items-center justify-between border-b-4 border-red-900 pb-4 mb-6">
        <div className="flex items-center gap-4">
          <span className="text-4xl">🕶️</span>
          <span className="text-2xl font-black orbitron text-red-600 tracking-widest uppercase">BLACK_OPS: DARK_RUN_V100</span>
        </div>
        <div className="text-xs text-red-500 bg-red-950/40 px-4 py-1 border-2 border-red-800 uppercase font-black italic flicker">SOVEREIGN_LEVEL_CLEARANCE</div>
      </div>

      <div className="flex-1 flex flex-col gap-6 min-h-0">
        <div className="flex-1 relative group bg-black border-4 border-red-950 shadow-[inset_0_0_30px_rgba(0,0,0,1)]">
          <textarea
            value={payload}
            onChange={(e) => setPayload(e.target.value)}
            className="w-full h-full bg-transparent p-6 font-mono text-xl text-red-500 font-bold outline-none focus:bg-red-950/5 transition-all custom-scrollbar resize-none placeholder:text-red-900"
            placeholder="SUBMIT BYTECODE ARTIFACT FOR STEALTH VALIDATION..."
          />
          {!payload && (
            <div className="absolute inset-0 flex items-center justify-center pointer-events-none opacity-5 select-none">
              <span className="text-6xl orbitron font-black text-red-600 tracking-[1em] rotate-12 uppercase">UNAUTHORIZED</span>
            </div>
          )}
        </div>

        <button
          onClick={executeTest}
          disabled={isRunning || !payload.trim()}
          className={`py-6 font-black orbitron text-2xl border-4 transition-all shadow-[8px_8px_0px_0px_rgba(0,0,0,1)] active:shadow-none active:translate-x-1 active:translate-y-1 ${
            isRunning || !payload.trim()
              ? 'bg-black text-red-950 border-red-950 cursor-not-allowed'
              : 'bg-red-700 text-white border-red-400 hover:bg-red-600'
          }`}
        >
          {isRunning ? 'RUNNING_DARK_SIMULATION...' : 'EXECUTE_BLACK_OPS_TEST'}
        </button>

        {result && (
          <div className={`p-6 border-4 shadow-[12px_12px_0px_0px_rgba(0,0,0,0.5)] animate-in zoom-in-95 duration-300 ${result.verdict === 'GO' ? 'border-green-600 bg-green-950/20' : 'border-red-600 bg-red-950/40'}`}>
            <div className="flex justify-between items-center mb-8">
              <div className="flex flex-col">
                <span className="text-xs text-gray-500 uppercase font-black tracking-widest mb-1">FINAL_VERDICT</span>
                <span className={`text-5xl font-black orbitron ${result.verdict === 'GO' ? 'text-green-500' : 'text-red-600 flicker'}`}>
                  {result.verdict}
                </span>
              </div>
              <div className="text-right">
                <div className="text-xs text-gray-500 uppercase font-black mb-1">Integrity_Score</div>
                <div className={`text-4xl orbitron font-black ${result.verdict === 'GO' ? 'text-green-500' : 'text-red-600'}`}>{result.integrity}%</div>
              </div>
            </div>

            <div className="grid grid-cols-2 gap-6 mb-8">
              <div className="bg-black border-4 border-red-950 p-4">
                <div className="text-[10px] text-gray-500 uppercase font-black tracking-tighter mb-2">Stealth_Rating</div>
                <div className="w-full h-4 bg-red-950">
                  <div className={`h-full ${result.verdict === 'GO' ? 'bg-green-600 shadow-[0_0_10px_rgba(0,255,65,0.5)]' : 'bg-red-600'}`} style={{ width: `${result.stealth}%` }}></div>
                </div>
              </div>
              <div className="bg-black border-4 border-red-950 p-4">
                <div className="text-[10px] text-gray-500 uppercase font-black tracking-tighter mb-2">Sovereignty_Lock</div>
                <div className={`text-xl font-black ${result.sovereignty ? 'text-green-500' : 'text-red-600'}`}>
                  {result.sovereignty ? 'ENFORCED_V100' : 'COMPROMISED'}
                </div>
              </div>
            </div>

            <div className="space-y-3 max-h-48 overflow-y-auto custom-scrollbar border-t-4 border-red-900/30 pt-4">
              <div className="text-xs text-gray-500 uppercase font-black mb-2">TRACE_ANALYSIS:</div>
              {result.leaks.length > 0 ? result.leaks.map((leak, i) => (
                <div key={i} className="text-sm font-bold flex items-start gap-4 text-red-500">
                  <span className="font-black">[!]</span>
                  <span>{leak}</span>
                </div>
              )) : (
                <div className="text-lg font-black text-green-500 italic animate-pulse">NO_SIGNATURE_TRACES_DETECTED // DARK_RUN_SAFE</div>
              )}
            </div>
          </div>
        )}
      </div>
    </div>
  );
};
