import {
    faAnglesLeft,
    faBars,
    faCloudArrowUp,
    faCogs,
    faDisplay,
    faDownLong,
    faHexagonNodes,
    faHouseChimneyUser,
    faLink,
    faList,
    faMobileVibrate,
    faPlug,
    faTableCellsLarge,
    faTableColumns,
    faWaveSquare,
} from "@fortawesome/free-solid-svg-icons";
import { FontAwesomeIcon } from "@fortawesome/react-fontawesome";
import { type JSX, memo, useEffect, useMemo, useRef, useState } from "react";
import { useTranslation } from "react-i18next";
import { NavLink, type NavLinkRenderProps } from "react-router";
import store2 from "store2";
import domLogo from "../images/dom_logo.png";
import Notifications from "../components/Notifications.js";
import PermitJoinButton from "../components/PermitJoinButton.js";
import { SIDEBAR_COLLAPSED_KEY } from "../localStoreConsts.js";
import NavBar from "./NavBar.js";
import { NavBarProvider } from "./NavBarContext.js";

type AppLayoutProps = {
    children: JSX.Element;
};

// XXX: `leading-none` prevents misalignment when sidebar collapsed/not collapsed (icon/icon+text)
const isNavActive = ({ isActive }: NavLinkRenderProps) => `py-2.5 leading-none lg:grid lg:tooltip lg:tooltip-right ${isActive ? "menu-active" : ""}`;

const AppLayout = memo(({ children }: AppLayoutProps) => {
    const { t } = useTranslation(["navbar", "common"]);
    const sidebarDrawerCheckboxRef = useRef<HTMLInputElement | null>(null);
    const [sidebarCollapsed, setSidebarCollapsed] = useState<boolean>(store2.get(SIDEBAR_COLLAPSED_KEY, false));
    const [showNotifications, setShowNotifications] = useState(false);

    useEffect(() => {
        store2.set(SIDEBAR_COLLAPSED_KEY, sidebarCollapsed);
    }, [sidebarCollapsed]);

    const links = useMemo(
        () => [
            { to: "/", icon: faHouseChimneyUser, title: t(($) => $.home) },
            { to: "/dashboard", icon: faTableColumns, title: t(($) => $.dashboard) },
            { to: "/devices", icon: faPlug, title: t(($) => $.devices) },
            { to: "/groups", icon: faTableCellsLarge, title: t(($) => $.groups) },
            { to: "/reporting", icon: faDownLong, title: t(($) => $.reporting) },
            { to: "/bindings", icon: faLink, title: t(($) => $.bindings) },
            { to: "/ota", icon: faCloudArrowUp, title: t(($) => $.ota) },
            { to: "/touchlink", icon: faMobileVibrate, title: t(($) => $.touchlink) },
            { to: "/network", icon: faHexagonNodes, title: t(($) => $.network) },
            { to: "/logs", icon: faList, title: t(($) => $.logs) },
            { to: "/activity", icon: faWaveSquare, title: t(($) => $.activity) },
            { to: "/settings", icon: faCogs, title: t(($) => $.settings) },
            { to: "/frontend-settings", icon: faDisplay, title: t(($) => $.frontend_settings) },
        ],
        [t],
    );

    const onSidebarLinkClick = () => {
        // don't change sidebar state on lg and up
        if (window.matchMedia("(min-width: 1024px)").matches) {
            return;
        }

        if (sidebarDrawerCheckboxRef.current) {
            sidebarDrawerCheckboxRef.current.checked = false;
        }
    };

    const linkLabelLargeClassName = `hidden ${sidebarCollapsed ? "lg:hidden" : "lg:inline"}`;

    return (
        <NavBarProvider>
            <div className="drawer lg:drawer-open">
                <input ref={sidebarDrawerCheckboxRef} id="sidebar-drawer" type="checkbox" className="drawer-toggle" />
                <div className="drawer-content">
                    <div className="drawer drawer-end w-auto">
                        <input id="notifications-drawer" type="checkbox" className="drawer-toggle" />
                        <div className="drawer-content flex flex-col min-h-screen">
                            <header className="sticky top-0 z-5">
                                <NavBar
                                    setSidebarCollapsed={setSidebarCollapsed}
                                    showNotifications={showNotifications}
                                    setShowNotifications={setShowNotifications}
                                />
                            </header>
                            <main className="pt-2 px-2 flex-1 min-h-0 scrollbar-gutter-stable">{children}</main>
                        </div>
                        <div className="drawer-side">
                            <label
                                htmlFor="notifications-drawer"
                                aria-label="close notifications"
                                className="drawer-overlay"
                                onClick={() => setShowNotifications(false)}
                            />
                            <aside className="bg-base-100 min-h-screen w-80">{showNotifications && <Notifications />}</aside>
                        </div>
                    </div>
                </div>
                <div className="drawer-side z-50 lg:overflow-visible!">
                    <label
                        htmlFor="sidebar-drawer"
                        aria-label="close sidebar"
                        className="drawer-overlay lg:hidden"
                        onClick={() => setSidebarCollapsed(true)}
                    />
                    <aside className={`bg-base-200 min-h-screen max-w-[85vw] ${sidebarCollapsed ? "lg:w-16" : "w-64 sm:w-72"}`}>
                        <nav className="flex flex-col h-full w-full overflow-y-auto max-h-screen pb-10">
                            <ul className="menu w-full p-2">
                                <li className="mb-2">
                                    <div
                                        className="flex items-center gap-3 px-2.5 py-2.5 cursor-pointer rounded-xl bg-purple-950/40 hover:bg-purple-900/40 border border-purple-500/20 transition-all select-none shadow-[0_2px_12px_rgba(168,85,247,0.15)]"
                                        onClick={() => {
                                            if (window.matchMedia("(min-width: 1024px)").matches) {
                                                setSidebarCollapsed(!sidebarCollapsed);
                                            }
                                        }}
                                        data-tip={sidebarCollapsed ? "DOM Connect v2.0" : "Collapse Sidebar"}
                                    >
                                        <img
                                            src={domLogo}
                                            alt="DOM Connect"
                                            className="w-8 h-8 object-contain filter drop-shadow-[0_0_12px_rgba(168,85,247,0.85)] flex-shrink-0 transition-transform hover:scale-110"
                                        />
                                        <div className={`flex flex-col min-w-0 ${sidebarCollapsed ? "hidden" : "flex"}`}>
                                            <div className="flex items-center gap-1.5">
                                                <span className="font-extrabold text-sm tracking-wide bg-gradient-to-r from-purple-200 via-fuchsia-300 to-violet-400 bg-clip-text text-transparent truncate">
                                                    DOM Connect
                                                </span>
                                                <span className="badge badge-xs bg-purple-600/30 text-purple-200 border border-purple-400/50 font-bold px-1.5 py-0.5 text-[9px] shadow-[0_0_8px_rgba(168,85,247,0.4)]">
                                                    v2.0
                                                </span>
                                            </div>
                                            <span className="text-[10px] text-purple-300/70 font-semibold tracking-wider uppercase truncate">
                                                Zigbee Gateway
                                            </span>
                                        </div>
                                        <button
                                            type="button"
                                            className={`ml-auto hidden ${sidebarCollapsed ? "" : "lg:block"} text-purple-300/40 hover:text-purple-200 p-1`}
                                            aria-label="Toggle Sidebar"
                                        >
                                            <FontAwesomeIcon icon={sidebarCollapsed ? faBars : faAnglesLeft} size="xs" />
                                        </button>
                                    </div>
                                </li>
                                {links.map((link) => (
                                    <li key={link.to}>
                                        <NavLink
                                            to={link.to}
                                            className={isNavActive}
                                            onClick={onSidebarLinkClick}
                                            data-tip={sidebarCollapsed ? link.title : ""}
                                        >
                                            <FontAwesomeIcon icon={link.icon} size="lg" />
                                            <span className="inline lg:hidden">{link.title}</span>
                                            <span className={linkLabelLargeClassName}>{link.title}</span>
                                        </NavLink>
                                    </li>
                                ))}
                            </ul>

                            <PermitJoinButton sidebarCollapsed={sidebarCollapsed} />
                        </nav>
                    </aside>
                </div>
            </div>
        </NavBarProvider>
    );
});

export default AppLayout;
