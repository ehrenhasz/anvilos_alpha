import React, { useState, useRef, useEffect } from 'react';
import axios from 'axios';
import { Terminal, Send } from 'lucide-react';

interface Message {
    role: 'user' | 'agent';
    message: string;
    timestamp: string;
}

export const ChatInterface: React.FC = () => {
    const [messages, setMessages] = useState<Message[]>([]);
    const [input, setInput] = useState('');
    const bottomRef = useRef<HTMLDivElement>(null);

    const fetchHistory = async () => {
        try {
            const res = await axios.get('/api/chat_history');
            setMessages(res.data);
        } catch (e) {
            console.error("History fetch failed", e);
        }
    };

    useEffect(() => {
        fetchHistory();
        const interval = setInterval(fetchHistory, 2000);
        return () => clearInterval(interval);
    }, []);

    useEffect(() => {
        bottomRef.current?.scrollIntoView({ behavior: 'smooth' });
    }, [messages]);

    const handleSend = async () => {
        if (!input.trim()) return;
        
        const userMsg = input;
        setInput('');

        try {
            await axios.post('/api/chat', { message: userMsg });
            // Let the polling pick it up
        } catch (e) {
            console.error("Send failed", e);
        }
    };

    return (
        <div style={{ 
            width: '75%', 
            backgroundColor: '#121212', 
            color: '#f0f0f0', 
            height: '100vh', 
            display: 'flex', 
            flexDirection: 'column' 
        }}>
            <div style={{ 
                padding: '20px', 
                borderBottom: '1px solid #333', 
                display: 'flex', 
                alignItems: 'center', 
                gap: '10px' 
            }}>
                <Terminal size={20} color="#00ff00" />
                <h2 style={{ margin: 0, fontSize: '1.2rem' }}>Aimeat Ops Terminal</h2>
            </div>

            <div style={{ flex: 1, overflowY: 'auto', padding: '20px', display: 'flex', flexDirection: 'column', gap: '15px' }}>
                {messages.map((msg, idx) => (
                    <div key={idx} style={{ 
                        alignSelf: msg.role === 'user' ? 'flex-end' : 'flex-start',
                        maxWidth: '80%',
                        backgroundColor: msg.role === 'user' ? '#0055ff' : '#2a2a2a',
                        padding: '12px 16px',
                        borderRadius: '8px',
                        lineHeight: '1.5',
                        fontFamily: 'monospace'
                    }}>
                        <div style={{ fontSize: '0.75rem', opacity: 0.6, marginBottom: '4px' }}>
                            {msg.role === 'user' ? 'YOU' : 'AGENT'}
                        </div>
                        {msg.message}
                    </div>
                ))}
                <div ref={bottomRef} />
            </div>

            <div style={{ padding: '20px', borderTop: '1px solid #333', display: 'flex', gap: '10px' }}>
                <input 
                    type="text" 
                    value={input}
                    onChange={(e) => setInput(e.target.value)}
                    onKeyDown={(e) => e.key === 'Enter' && handleSend()}
                    placeholder="Enter command..."
                    style={{
                        flex: 1,
                        padding: '12px',
                        borderRadius: '4px',
                        border: '1px solid #444',
                        backgroundColor: '#1e1e1e',
                        color: 'white',
                        fontFamily: 'monospace'
                    }}
                />
                <button 
                    onClick={handleSend}
                    style={{
                        padding: '0 20px',
                        backgroundColor: '#00ff00',
                        color: 'black',
                        border: 'none',
                        borderRadius: '4px',
                        fontWeight: 'bold',
                        cursor: 'pointer'
                    }}
                >
                    <Send size={18} />
                </button>
            </div>
        </div>
    );
};
