import { faBars, faInbox } from "@fortawesome/free-solid-svg-icons";
import { FontAwesomeIcon } from "@fortawesome/react-fontawesome";
import { type Dispatch, memo, type SetStateAction } from "react";
import { useTranslation } from "react-i18next";
import { useAppStore } from "../store.js";
import { useNavBarContent } from "./NavBarContext.js";
import domLogo from "../images/dom_logo.png";

type NavBarProps = {
    setSidebarCollapsed: Dispatch<SetStateAction<boolean>>;
    showNotifications: boolean;
    setShowNotifications: Dispatch<SetStateAction<boolean>>;
};

const NavBar = memo(({ setSidebarCollapsed, showNotifications, setShowNotifications }: NavBarProps) => {
    const content = useNavBarContent();
    const { t } = useTranslation("common");

    const notificationsAlert = useAppStore((state) => state.notificationsAlert);

    return (
        <div className="navbar bg-base-200 shadow w-full min-h-12 px-2 sm:px-4 py-1.5">
            <div className="flex flex-row items-center gap-1.5 sm:gap-2 flex-1 min-w-0">
                <label
                    htmlFor="sidebar-drawer"
                    id="mobile-drawer-toggle-btn"
                    className="btn btn-ghost btn-sm text-purple-200 hover:text-white hover:bg-purple-900/40 border border-purple-500/20 lg:hidden flex-shrink-0 p-1.5 shadow-[0_0_8px_rgba(168,85,247,0.25)] cursor-pointer"
                    aria-label="show sidebar"
                    onClick={() => setSidebarCollapsed(false)}
                >
                    <FontAwesomeIcon icon={faBars} size="lg" />
                </label>
                <div className="flex items-center gap-1.5 lg:hidden flex-shrink-0">
                    <img src={domLogo} alt="DOM Connect" className="w-6 h-6 object-contain filter drop-shadow-[0_0_10px_rgba(168,85,247,0.9)]" />
                    <span className="font-extrabold text-xs sm:text-sm bg-gradient-to-r from-purple-200 via-fuchsia-300 to-violet-400 bg-clip-text text-transparent hidden sm:inline">
                        DOM Connect
                    </span>
                </div>
                <div className="min-w-0 flex-1 overflow-x-auto no-scrollbar">
                    {content}
                </div>
            </div>

            <div className="flex flex-row items-center gap-1 flex-shrink-0 ms-1 sm:ms-2">
                <label
                    htmlFor="notifications-drawer"
                    className="drawer-button btn btn-sm btn-outline btn-primary tooltip tooltip-left"
                    data-tip={t(($) => $.notifications)}
                    aria-label="toggle notifications"
                    onClick={() => setShowNotifications(!showNotifications)}
                >
                    <FontAwesomeIcon icon={faInbox} />
                    {notificationsAlert[0] ? (
                        <span className="status status-primary animate-bounce" />
                    ) : notificationsAlert[1] ? (
                        <span className="status status-error animate-bounce" />
                    ) : null}
                </label>
            </div>
        </div>
    );
});

export default NavBar;
