import { createEventSignal } from "@solid-primitives/event-listener";
import { createReconnectingWS, createWSState } from "@solid-primitives/websocket";
import { batch, createContext, createEffect, createSignal, type JSX, useContext } from "solid-js";
import { createStore } from "solid-js/store";

import { type ScheduleItem, type Store, type StoreActions, SYSTEM_STATUS } from "../types";
import { ToastProvider } from "./toast";

const ws = createReconnectingWS(
  `${import.meta.env.PROD ? `ws://${window.location.host}/` : import.meta.env.VITE_WS_URL}ws`,
);

const wsState = createWSState(ws);

const connectionStatus = ["Connecting", "Connected", "Disconnecting", "Disconnected"];
const WS_CONNECTED = 1;
const WS_DISCONNECTED = 3;

const [mainStore, setStore] = createStore<Store>({
  isActiveScheduler: false,
  rotation: 0,
  plugins: [],
  plugin: 1,
  brightness: 0,
  artnetUniverse: 1,
  indexMatrix: [...new Array(256)].map((_, i) => i),
  leds: [...new Array(256)].fill(0),
  systemStatus: SYSTEM_STATUS.NONE,
  connectionState: wsState,
  connectionStatus: connectionStatus[0],
  schedule: [],
});

const actions: StoreActions = {
  setIsActiveScheduler: (isActive) => setStore("isActiveScheduler", isActive),
  setRotation: (rotation) => setStore("rotation", rotation),
  setPlugins: (plugins) => setStore("plugins", plugins),
  setPlugin: (plugin) => setStore("plugin", plugin),
  setBrightness: (brightness) => setStore("brightness", brightness),
  setArtnetUniverse: (artnetUniverse) => setStore("artnetUniverse", artnetUniverse),
  setIndexMatrix: (indexMatrix) => setStore("indexMatrix", indexMatrix),
  setLeds: (leds) => setStore("leds", leds),
  setSystemStatus: (systemStatus: SYSTEM_STATUS) => setStore("systemStatus", systemStatus),
  setSchedule: (items: ScheduleItem[]) => setStore("schedule", items),
  send: ws.send,
};

const store: [Store, StoreActions] = [mainStore, actions] as const;

const StoreContext = createContext<[Store, StoreActions]>(store);

export const StoreProvider = (props?: { value?: Store; children?: JSX.Element }) => {
  const messageEvent = createEventSignal<{ message: MessageEvent }>(ws, "message");
  const [hasEverConnected, setHasEverConnected] = createSignal(false);
  const [sawUpdateStatus, setSawUpdateStatus] = createSignal(false);
  const [reloadOnReconnect, setReloadOnReconnect] = createSignal(false);

  createEffect(() => {
    const state = wsState();

    setStore("connectionStatus", connectionStatus[state] || "Unknown");

    if (state === WS_CONNECTED) {
      setHasEverConnected(true);

      // If the device rebooted after OTA, refresh once so all resources and
      // initial state are reloaded from the freshly booted firmware.
      if (reloadOnReconnect()) {
        setReloadOnReconnect(false);
        window.location.reload();
        return;
      }
    }

    if (state === WS_DISCONNECTED && hasEverConnected() && sawUpdateStatus()) {
      setReloadOnReconnect(true);
    }
  });

  createEffect(() => {
    const json = JSON.parse(messageEvent()?.data || "{}");

    switch (json.event) {
      case "info":
        batch(() => {
          actions.setSystemStatus(Object.values(SYSTEM_STATUS)[json.status as number]);
          setSawUpdateStatus(Object.values(SYSTEM_STATUS)[json.status as number] === SYSTEM_STATUS.UPDATE);
          actions.setRotation(json.rotation);
          actions.setBrightness(json.brightness);
          actions.setIsActiveScheduler(json.scheduleActive);

          if (json.schedule) {
            actions.setSchedule(json.schedule);
          }

          if (!mainStore.plugins.length) {
            actions.setPlugins(json.plugins);
          }

          if (json.plugin) {
            actions.setPlugin(json.plugin as number);
          }

          if (mainStore.plugin === 1) {
            actions.setIndexMatrix([...new Array(256)].map((_, i) => i));
          }

          if (json.data) {
            actions.setLeds(json.data);
          }
        });
        break;
    }
  });

  return (
    <ToastProvider>
      <StoreContext.Provider value={store}>{props?.children}</StoreContext.Provider>
    </ToastProvider>
  );
};

export const useStore = () => {
  if (StoreContext) return useContext(StoreContext);

  return [{}, {}] as [Store, StoreActions];
};
