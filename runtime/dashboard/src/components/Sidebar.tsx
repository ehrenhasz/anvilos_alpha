import React, { useEffect, useState } from 'react';
import { fetchQueue } from '../api';
import { Library, Loader2 } from 'lucide-react';

export const Sidebar: React.FC = () => {
    const [queue, setQueue] = useState<any[]>([]);

    useEffect(() => {
        // Mock polling
        const interval = setInterval(async () => {
            const data = await fetchQueue();
            setQueue(data);
        }, 3000);
        return () => clearInterval(interval);
    }, []);

    return (
        <div style={{ 
            width: '25%', 
            backgroundColor: '#1e1e1e', 
            color: '#e0e0e0', 
            borderRight: '1px solid #333',
            height: '100vh',
            display: 'flex',
            flexDirection: 'column'
        }}>
            <div style={{ padding: '20px', borderBottom: '1px solid #333', display: 'flex', alignItems: 'center', gap: '10px' }}>
                <Library size={20} />
                <h2 style={{ margin: 0, fontSize: '1.2rem' }}>Librarian Queue</h2>
            </div>
            <div style={{ flex: 1, overflowY: 'auto', padding: '10px' }}>
                {queue.length === 0 ? (
                    <p style={{ color: '#777', textAlign: 'center' }}>Queue Empty</p>
                ) : (
                    queue.map((item) => (
                        <div key={item.id} style={{ 
                            padding: '10px', 
                            marginBottom: '8px', 
                            backgroundColor: '#2a2a2a', 
                            borderRadius: '4px',
                            fontSize: '0.9rem'
                        }}>
                            <div style={{ fontWeight: 'bold', marginBottom: '4px' }}>{item.filename}</div>
                            <div style={{ display: 'flex', alignItems: 'center', gap: '6px', fontSize: '0.8rem', color: item.status === 'PROCESSING' ? '#4caf50' : '#ff9800' }}>
                                {item.status === 'PROCESSING' && <Loader2 size={12} className="spin" />}
                                {item.status}
                            </div>
                        </div>
                    ))
                )}
            </div>
        </div>
    );
};
