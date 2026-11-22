import * as React from 'react';
import { createStore } from 'zustand/vanilla';
import { useStore } from 'zustand';

const ExampleContext = React.createContext(null);

export function createExampleStore() {
  return createStore((set) => ({
    note: '',
    visits: 0,
    setNote: (note) => set({ note }),
    increment: () => set((state) => ({ visits: state.visits + 1 })),
    reset: () => set({ note: '', visits: 0 }),
  }));
}

// One store per mounted application. A second app/root cannot borrow the first
// app's draft, and route remounts keep the root's store until that root closes.
export function ExampleProvider({ children }) {
  const store = React.useRef(null);
  if (store.current === null) store.current = createExampleStore();
  return <ExampleContext.Provider value={store.current}>{children}</ExampleContext.Provider>;
}

export function useExample(selector) {
  const store = React.useContext(ExampleContext);
  if (!store) throw new Error('Example screens must be inside ExampleProvider');
  return useStore(store, selector);
}
