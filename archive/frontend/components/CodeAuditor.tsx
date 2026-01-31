
import React, { useState } from 'react';
import { auditCodeSovereignty } from '../services/geminiService';
import { AuditResult, BicameralConfig } from '../types';

interface CodeAuditorProps {
  config: BicameralConfig;
}

export const CodeAuditor: React.FC<CodeAuditorProps> = ({ config }) => {
  const [code, setCode] = useState('');
  const [isAuditing, setIsAuditing] = useState(false);
  const [result, setResult] = useState<AuditResult | null>(null);

  const runAudit = async () => {
    if (!code.trim()) return;
    setIsAuditing(true);
    setResult(null);
    const audit = await auditCodeSovereignty(code, config);
    setResult(audit);
    setIsAuditing(false);
  };

  return (
    <div className="flex flex-col h-full bg-[#000000] p-4 relative overflow-hidden border border-green-900/50">
      <div className="flex items-center justify-between border-b border-green-900 pb-3 mb-4">
        <div className="flex items-center gap-3">
          <div className="w-3 h-3 bg-green-500 animate-pulse shadow-[0_0_8px_rgba(0,255,65,1)]"></div>
          <span className="text-xs font-black orbitron text-green-500 uppercase tracking-widest">ANVIL: VirtualMeat_AUDIT_v10.0</span>
        </div>
        <div className="text-[8px] text-green-400 bg-green-950/40 px-2 border border-green-900 uppercase font-black tracking-widest italic animate-flicker">Logic_Lobotomy_Ready</div>
      </div>

      <div className="flex-1 flex flex-col gap-4 min-h-0">
        <div className="flex-1 relative border border-green-900/50 bg-green-950/5 group">
          <div className="absolute top-0 right-0 p-1 text-[7px] text-green-900 font-bold opacity-30 uppercase">Soul_Ingestion_Zone</div>
          <textarea
            value={code}
            onChange={(e) => setCode(e.target.value)}
            className="w-full h-full bg-transparent p-4 font-mono text-xs text-green-500 outline-none focus:bg-green-500/5 transition-all custom-scrollbar resize-none placeholder:text-green-950"
            placeholder="Paste high-level source (The Soul) for VirtualMeat processing..."
          />
        </div>

        <button
          onClick={runAudit}
          disabled={isAuditing || !code.trim()}
          className={`py-4 font-black orbitron text-sm border-2 transition-all shadow-[4px_4px_0px_0px_rgba(0,0,0,1)] ${
            isAuditing || !code.trim()
              ? 'bg-black text-green-950 border-green-950 cursor-not-allowed'
              : 'bg-green-600 text-black border-green-400 hover:bg-green-500 hover:translate-x-0.5 hover:translate-y-0.5 hover:shadow-none active:scale-95'
          }`}
        >
          {isAuditing ? 'STRIPPING_METADATA...' : 'EXECUTE_VIRTUALMEAT_AUDIT'}
        </button>

        {result && (
          <div className={`p-4 border shadow-[8px_8px_0px_0px_rgba(0,0,0,0.5)] animate-in slide-in-from-bottom-4 duration-300 ${result.passed ? 'border-green-500 bg-green-950/10' : 'border-red-600 bg-red-950/20'}`}>
            <div className="flex justify-between items-center mb-4">
              <span className="text-[9px] font-black orbitron text-green-700 uppercase tracking-widest">Audit_Report_v99</span>
              <div className="flex items-center gap-2">
                <span className={`text-xl font-black orbitron ${result.passed ? 'text-green-500' : 'text-red-600 flicker'}`}>
                  {result.passed ? 'PASSED_CLEAN' : 'CONTAMINATED'}
                </span>
              </div>
            </div>
            
            <div className="grid grid-cols-2 gap-4 mb-4">
              <div className="bg-black border border-green-900/30 p-2">
                <div className="text-[7px] text-green-900 uppercase font-black mb-1">Sovereignty_Index</div>
                <div className="text-lg orbitron font-bold text-green-500">{result.score}%</div>
              </div>
              <div className="bg-black border border-green-900/30 p-2">
                <div className="text-[7px] text-green-900 uppercase font-black mb-1">RFC-0009_Test</div>
                <div className={`text-lg orbitron font-bold ${result.protocolV5Compliance ? 'text-green-500' : 'text-red-500'}`}>
                  {result.protocolV5Compliance ? 'VALID' : 'FAILED'}
                </div>
              </div>
            </div>

            <div className="space-y-1 overflow-y-auto max-h-32 custom-scrollbar border-t border-green-900/30 pt-2">
              <div className="text-[8px] text-green-900 uppercase mb-2 font-black">VirtualMeat_Trace_Analysis:</div>
              {result.findings.map((f, i) => (
                <div key={i} className="text-[9px] font-mono flex items-start gap-2 text-green-600/70">
                  <span className="text-red-600 font-bold">!</span>
                  <span>{f}</span>
                </div>
              ))}
            </div>
          </div>
        )}
      </div>
    </div>
  );
};
