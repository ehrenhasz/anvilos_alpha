
import React, { useState } from 'react';
import { SystemFile, BicameralConfig } from '../types';

interface VaultProps {
  config: BicameralConfig;
}

export const Vault: React.FC<VaultProps> = ({ config }) => {
  const [files, setFiles] = useState<SystemFile[]>([
    { id: '1', name: 'orbital_drop_v5.py', size: '12KB', type: 'Python', timestamp: '2026-01-11 14:00' },
    { id: '2', name: 'drop_manifest.json', size: '2KB', type: 'JSON', timestamp: '2026-01-11 14:05' },
    { id: '3', name: 'agent_sovereignty.md', size: '4KB', type: 'Markdown', timestamp: '2026-01-11 15:30' },
    { id: '4', name: 'pqc_identity_key', size: '1KB', type: 'Secret', timestamp: '2026-01-11 16:45' },
  ]);

  const handleLocalUpload = (e: React.ChangeEvent<HTMLInputElement>) => {
    const uploaded = e.target.files;
    if (!uploaded) return;

    Array.from(uploaded).forEach((file: File) => {
      const newFile: SystemFile = {
        id: Math.random().toString(36).substr(2, 9),
        name: file.name,
        size: `${Math.round(file.size / 1024)}KB`,
        type: file.name.split('.').pop()?.toUpperCase() || 'UNKNOWN',
        timestamp: new Date().toLocaleString()
      };
      setFiles(prev => [newFile, ...prev]);
    });
  };

  const cardBorder = config.highVisibility ? 'border-2' : 'border';
  const textClass = config.highVisibility ? 'text-lg font-black' : 'text-xs';

  return (
    <div className={`flex flex-col h-full bg-black/40 p-4 overflow-hidden transition-all ${config.highVisibility ? 'p-10' : 'p-4'}`}>
      <div className="flex items-center justify-between border-b border-green-900 pb-3 mb-4">
        <div className="flex items-center gap-3">
          <span className={`${config.highVisibility ? 'text-4xl' : 'text-xl'}`}>📁</span>
          <h2 className={`font-black orbitron text-green-500 uppercase ${config.highVisibility ? 'text-2xl' : 'text-base'}`}>INTERNAL_VAULT</h2>
        </div>
        <label className={`bg-green-600 text-black px-4 py-2 font-black orbitron cursor-pointer hover:bg-green-500 transition-all flex items-center justify-center ${config.highVisibility ? 'text-xl px-8 py-4 border-4' : 'text-[10px] border'}`}>
          INGEST_ARTIFACT
          <input type="file" className="hidden" onChange={handleLocalUpload} multiple />
        </label>
      </div>

      <div className="flex-1 overflow-y-auto custom-scrollbar pr-2 grid grid-cols-1 md:grid-cols-2 gap-4 pb-4">
        {files.map(file => (
          <div key={file.id} className={`bg-black border border-green-900 p-4 flex flex-col gap-3 group hover:border-green-500 transition-all ${cardBorder}`}>
            <div className="flex justify-between items-start">
              <span className="text-2xl opacity-50">📄</span>
              <div className="text-right">
                <span className="text-[8px] font-black text-green-900 uppercase">Fmt:: {file.type}</span>
                <div className="text-sm font-black text-green-400">{file.size}</div>
              </div>
            </div>
            
            <div className="flex-1">
              <h3 className={`text-green-500 break-all mb-1 font-bold ${config.highVisibility ? 'text-xl' : 'text-sm'}`}>{file.name}</h3>
              <p className="text-[8px] text-green-800 font-bold uppercase">{file.timestamp}</p>
            </div>
            
            <div className="flex gap-2 pt-3 border-t border-green-900/30">
              <button className={`flex-1 bg-green-900/10 py-1.5 text-[9px] font-black border border-green-800 hover:bg-green-600 hover:text-black transition-all ${config.highVisibility ? 'text-sm py-3' : ''}`}>VIEW</button>
              <button className={`flex-1 bg-red-950/10 py-1.5 text-[9px] font-black border border-red-900 text-red-700 hover:bg-red-600 hover:text-white transition-all ${config.highVisibility ? 'text-sm py-3' : ''}`}>PURGE</button>
            </div>
          </div>
        ))}
      </div>

      <div className="mt-4 p-3 bg-green-950/20 border border-green-900/40 flex justify-between text-[9px] font-black uppercase opacity-60">
        <span>Artifact_count: {files.length}</span>
        <span>Registry: Sovereignty_Verified</span>
      </div>
    </div>
  );
};
