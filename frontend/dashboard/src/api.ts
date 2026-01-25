import axios from 'axios';

const API_BASE = '/api';

export const fetchQueue = async () => {
    try {
        const res = await axios.get(`${API_BASE}/queue`);
        return res.data;
    } catch (e) {
        // Fallback for demo/offline
        return [
            { id: 1, filename: "handbook_v1.pdf", status: "PROCESSING" },
            { id: 2, filename: "unknown_map.jpg", status: "QUEUED" },
        ];
    }
};

export const sendChat = async (message: string) => {
    try {
        const res = await axios.post(`${API_BASE}/chat`, { message });
        return res.data;
    } catch (e) {
        console.error(e);
        return { response: "Ack. Message received (Offline Mode)." };
    }
};