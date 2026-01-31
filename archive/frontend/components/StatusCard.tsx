
import React from 'react';

interface StatusCardProps {
  label: string;
  value: string;
  pulse?: boolean;
}

export const StatusCard: React.FC<StatusCardProps> = ({ label, value, pulse }) => {
  return (
    <div className="border border-green-900/50 bg-black/40 p-3 rounded-sm relative overflow-hidden group">
      <div className="text-[10px] text-green-700 uppercase tracking-widest mb-1">{label}</div>
      <div className={`text-lg orbitron font-bold ${pulse ? 'animate-pulse text-green-400' : 'text-green-500'}`}>
        {value}
      </div>
      <div className="absolute top-0 right-0 w-1 h-full bg-green-500/20 group-hover:bg-green-500/50 transition-colors"></div>
    </div>
  );
};
