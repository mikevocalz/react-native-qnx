/** Shared native starter; layout is handled by React Native and Yoga. */
import React from 'react';
import { AppRegistry, View, Text, Pressable, ScrollView, TextInput } from 'react-native';
import {
  AdaptiveShell,
  AdaptiveGrid,
  defaultAdaptiveColors,
} from '@rn-qnx/react-native-qnx/adaptive';
import { create } from 'zustand';

const h = React.createElement;
const APP_TITLE = '__APP_NAME__ on React Native QNX';
const colors = defaultAdaptiveColors;
const navigation = [
  { id: 'home', label: 'Home' },
  { id: 'about', label: 'About' },
];
const useApp = create((set) => ({
  page: 'home',
  count: 0,
  text: 'Hello, QNX.',
  select: (page) => set({ page }),
  increment: () => set((state) => ({ count: state.count + 1 })),
  edit: (text) => set({ text }),
}));
const panel = {
  padding: 20,
  borderRadius: 6,
  backgroundColor: colors.surface,
  borderWidth: 1,
  borderColor: colors.border,
};
const heading = { color: colors.text, fontSize: 21, lineHeight: 28, marginBottom: 16 };
const body = { color: colors.text, fontSize: 16, lineHeight: 23 };

function PageLayer({ active, children }) {
  return h(
    View,
    {
      collapsable: false,
      pointerEvents: active ? 'auto' : 'none',
      accessibilityElementsHidden: !active,
      importantForAccessibility: active ? 'auto' : 'no-hide-descendants',
      style: {
        position: 'absolute',
        top: 0,
        right: 0,
        bottom: 0,
        left: 0,
        opacity: active ? 1 : 0,
      },
    },
    children
  );
}

function App() {
  const page = useApp((state) => state.page);
  const count = useApp((state) => state.count);
  const text = useApp((state) => state.text);
  const select = useApp((state) => state.select);
  const increment = useApp((state) => state.increment);
  const edit = useApp((state) => state.edit);
  return h(
    AdaptiveShell,
    { title: APP_TITLE, navigation, selectedId: page, onSelect: select },
    // Cached routes keep their native nodes and geometry when inactive.
    // display:none would make Fabric remove their native subtrees.
    h(
      View,
      { collapsable: false, style: { flex: 1, minHeight: 0, minWidth: 0 } },
      h(
        PageLayer,
        { active: page === 'home' },
        h(
          ScrollView,
          { style: { flex: 1 }, contentContainerStyle: { paddingBottom: 16 } },
          h(
            AdaptiveGrid,
            { minItemWidth: 300, maxColumns: 2 },
            h(
              View,
              { key: 'action', style: panel },
              h(Text, { style: heading }, 'Your workspace'),
              h(
                Pressable,
                {
                  onPress: increment,
                  accessibilityRole: 'button',
                  accessibilityLabel: 'Increase count',
                  style: {
                    alignSelf: 'flex-start',
                    minHeight: 44,
                    padding: 12,
                    borderRadius: 5,
                    backgroundColor: colors.selected,
                  },
                },
                h(Text, { style: { ...body, color: colors.selectedText } }, 'Count: ' + count)
              )
            ),
            h(
              View,
              { key: 'input', style: panel },
              h(Text, { style: heading }, 'Make it yours'),
              h(TextInput, {
                value: text,
                onChangeText: edit,
                accessibilityLabel: 'Example text',
                style: {
                  ...body,
                  minHeight: 48,
                  padding: 12,
                  borderRadius: 5,
                  borderWidth: 1,
                  borderColor: colors.border,
                  backgroundColor: colors.background,
                },
              })
            )
          )
        )
      ),
      h(
        PageLayer,
        { active: page === 'about' },
        h(
          ScrollView,
          { style: { flex: 1 } },
          h(
            View,
            { style: { maxWidth: 640 } },
            h(Text, { style: heading }, 'Ready for different window sizes'),
            h(
              Text,
              { style: body },
              'The navigation changes with the window. Content columns follow the space they actually have. Your application owns its state.'
            )
          )
        )
      )
    )
  );
}

// This name matches the existing native playground surface entry point.
AppRegistry.registerComponent('RNQnxPlayground', () => App);
